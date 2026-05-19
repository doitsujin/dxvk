#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_presenter.h"

#include "../wsi/wsi_window.h"

namespace dxvk {

  const std::array<std::pair<VkColorSpaceKHR, VkColorSpaceKHR>, 2> Presenter::s_colorSpaceFallbacks = {{
    { VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, VK_COLOR_SPACE_HDR10_ST2084_EXT },

    { VK_COLOR_SPACE_HDR10_ST2084_EXT, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT },
  }};


  Presenter::Presenter(
    const Rc<DxvkDevice>&   device,
    const Rc<sync::Signal>& signal,
    const PresenterDesc&    desc,
          PresenterSurfaceProc&& proc)
  : m_device(device), m_signal(signal),
    m_vki(device->instance()->vki()),
    m_vkd(device->vkd()),
    m_surfaceProc(std::move(proc)) {
    // Only enable FSE if the user explicitly opts in. On Windows, FSE
    // is required to support VRR or HDR, but blocks alt-tabbing or
    // overlapping windows, which breaks a number of games.
    m_fullscreenMode = m_device->config().allowFse
      ? VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT
      : VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;

    // Create Vulkan surface immediately if possible, but ignore
    // certain errors since the app window may still be in use in
    // some way at this point, e.g. by a different device.
    if (!desc.deferSurfaceCreation) {
      VkResult vr = createSurface();

      if (vr != VK_SUCCESS && vr != VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
        throw DxvkError(str::format("Failed to create Vulkan surface, ", vr));
    }

    // KHR and EXT variants of this extension are completely identical
    m_hasSwapchainMaintenance1 = m_device->features().khrSwapchainMaintenance1.swapchainMaintenance1
                              || m_device->features().extSwapchainMaintenance1.swapchainMaintenance1;

    // Gamescope WSI is currently broken and doesn't properly signal
    // the present fence if presentation is queued but fails.
    // TODO Remove this hack when this gets fixed in stable SteamOS.
    m_hasGamescopeFenceSignalBug = env::getEnvVar("ENABLE_GAMESCOPE_WSI") == "1";
  }

  
  Presenter::~Presenter() {
    destroySwapchain();
    destroySurface();
    destroyLatencySemaphore();

    if (m_frameThread.joinable()) {
      pushFrame(PresenterFrame());
      m_frameThread.join();
    }
  }


  VkResult Presenter::checkSwapChainStatus() {
    std::lock_guard lock(m_surfaceMutex);

    if (!m_swapchain)
      return recreateSwapChain();

    return VK_SUCCESS;
  }


  VkResult Presenter::acquireNextImage(PresenterSync& sync, Rc<DxvkImage>& image) {
    std::unique_lock lock(m_surfaceMutex);

    // Don't acquire more than one image at a time
    VkResult status = VK_SUCCESS;

    m_surfaceCond.wait(lock, [this, &status] {
      status = m_device->getDeviceStatus();
      return !m_presentPending || status < 0;
    });

    if (status < 0)
      return status;

    // Ensure that the swap chain gets recreated if it is dirty
    bool hasSwapchain = m_swapchain != VK_NULL_HANDLE;

    updateSwapChain();

    // Don't acquire if we already did so after present
    if (m_acquireStatus == VK_NOT_READY && m_swapchain) {
      PresenterSync sync = m_semaphores.at(m_frameIndex);

      waitForSwapchainFence(sync);

      m_acquireStatus = m_vkd->vkAcquireNextImageKHR(m_vkd->device(),
        m_swapchain, std::numeric_limits<uint64_t>::max(),
        sync.acquire, VK_NULL_HANDLE, &m_imageIndex);
    }

    // This is a normal occurence, but may be useful for
    // debugging purposes in case WSI goes wrong somehow.
    if (m_acquireStatus != VK_SUCCESS && m_swapchain)
      Logger::info(str::format("Presenter: Got ", m_acquireStatus, ", recreating swapchain"));

    // If the swap chain is out of date, recreate it and retry. It
    // is possible that we do not get a new swap chain here, e.g.
    // because the window is minimized.
    if (m_acquireStatus != VK_SUCCESS || !m_swapchain) {
      VkResult vr = recreateSwapChain();

      if (vr == VK_NOT_READY && hasSwapchain)
        Logger::info("Presenter: Surface does not allow swapchain creation.");

      if (vr != VK_SUCCESS)
        return softError(vr);

      PresenterSync sync = m_semaphores.at(m_frameIndex);

      m_acquireStatus = m_vkd->vkAcquireNextImageKHR(m_vkd->device(),
        m_swapchain, std::numeric_limits<uint64_t>::max(),
        sync.acquire, VK_NULL_HANDLE, &m_imageIndex);

      if (m_acquireStatus < 0) {
        Logger::info(str::format("Presenter: Got ", m_acquireStatus, " from fresh swapchain"));
        return softError(m_acquireStatus);
      }
    }

    // Update HDR metadata after a successful acquire. We know
    // that there won't be a present in flight at this point.
    if (m_hdrMetadataDirty && m_hdrMetadata) {
      m_hdrMetadataDirty = false;

      if (m_device->features().extHdrMetadata) {
        m_vkd->vkSetHdrMetadataEXT(m_vkd->device(),
          1, &m_swapchain, &(*m_hdrMetadata));
      }
    }

    // Apply latency sleep mode if the swapchain supports it
    if (m_latencySleepModeDirty && m_latencySleepMode) {
      m_latencySleepModeDirty = false;

      if (m_latencySleepSupported) {
        m_vkd->vkSetLatencySleepModeNV(m_vkd->device(),
          m_swapchain, &(*m_latencySleepMode));
      }
    }

    // Set dynamic present mode for the next frame if possible
    if (!m_dynamicModes.empty())
      m_presentMode = m_dynamicModes.at(m_preferredSyncInterval ? 1u : 0u); 

    // Return relevant Vulkan objects for the acquired image
    sync = m_semaphores.at(m_frameIndex);
    image = m_images.at(m_imageIndex);

    m_presentPending = true;
    return m_acquireStatus;
  }


  VkResult Presenter::presentImage(uint64_t frameId, const Rc<DxvkLatencyTracker>& tracker) {
    PresenterSync& currSync = m_semaphores.at(m_frameIndex);

    uint64_t frameDeadline = 0u;

    VkPresentIdKHR presentId = { VK_STRUCTURE_TYPE_PRESENT_ID_KHR };
    presentId.swapchainCount = 1;
    presentId.pPresentIds   = &frameId;

    VkPresentId2KHR presentId2 = { VK_STRUCTURE_TYPE_PRESENT_ID_2_KHR };
    presentId2.swapchainCount = 1;
    presentId2.pPresentIds  = &frameId;

    VkSwapchainPresentFenceInfoKHR fenceInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR };
    fenceInfo.swapchainCount = 1;
    fenceInfo.pFences       = &currSync.fence;

    VkSwapchainPresentModeInfoKHR modeInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_KHR };
    modeInfo.swapchainCount = 1;
    modeInfo.pPresentModes  = &m_presentMode;

    // Present timing isn't useful with Immediate or Mailbox
    bool isFifoMode = m_presentMode == VK_PRESENT_MODE_FIFO_KHR
                   || m_presentMode == VK_PRESENT_MODE_FIFO_RELAXED_KHR;

    VkPresentTimingInfoEXT timingInfo = { VK_STRUCTURE_TYPE_PRESENT_TIMING_INFO_EXT };
    timingInfo.presentStageQueries = m_timingMode.presentStage;

    if (m_timingMode.presentStage && isFifoMode) {
      std::lock_guard lock(m_timingMutex);

      if (m_timingMode.relativeTiming) {
        timingInfo.flags |= VK_PRESENT_TIMING_INFO_PRESENT_AT_RELATIVE_TIME_BIT_EXT;
        timingInfo.targetTime = m_timingMode.frameIntervalNs;
        timingInfo.targetTimeDomainPresentStage = m_timingMode.presentStage;
        timingInfo.timeDomainId = m_timingMode.timeDomainId;
      } else if (m_timingMode.absoluteTiming && m_timingMode.referenceFrameId) {
        timingInfo.targetTime = m_timingMode.referenceTime + (frameId - m_timingMode.referenceFrameId) * m_timingMode.frameIntervalNs;
        timingInfo.targetTimeDomainPresentStage = m_timingMode.presentStage;
        timingInfo.timeDomainId = m_timingMode.timeDomainId;

        if (m_timingDisplayInfo && !m_timingDisplayInfo->isVariableRefresh)
          timingInfo.flags |= VK_PRESENT_TIMING_INFO_PRESENT_AT_NEAREST_REFRESH_CYCLE_BIT_EXT;

        frameDeadline = timingInfo.targetTime + m_timingMode.frameIntervalNs;
      }
    }

    VkPresentTimingsInfoEXT timingsInfo = { VK_STRUCTURE_TYPE_PRESENT_TIMINGS_INFO_EXT };
    timingsInfo.swapchainCount = 1u;
    timingsInfo.pTimingInfos = &timingInfo;

    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &currSync.present;
    info.swapchainCount     = 1;
    info.pSwapchains        = &m_swapchain;
    info.pImageIndices      = &m_imageIndex;

    if (frameId && m_hasPresentId) {
      if (m_device->features().khrPresentId2.presentId2)
        presentId2.pNext = const_cast<void*>(std::exchange(info.pNext, &presentId2));
      else
        presentId.pNext = const_cast<void*>(std::exchange(info.pNext, &presentId));

      if (timingInfo.presentStageQueries)
        timingsInfo.pNext = std::exchange(info.pNext, &timingsInfo);
    }

    if (m_hasSwapchainMaintenance1) {
      modeInfo.pNext = const_cast<void*>(std::exchange(info.pNext, &modeInfo));
      fenceInfo.pNext = const_cast<void*>(std::exchange(info.pNext, &fenceInfo));
    }

    VkResult status = m_vkd->vkQueuePresentKHR(
      m_device->queues().graphics.queueHandle, &info);

    // Treat a QUEUE_FULL error as a hint to recreate the swapchain with
    // a larger queue size. This could probably be done more robustly,
    // but since we have latency constraints in the frontend APIs, we
    // should never run into this scenario in practice anyway.
    if (status == VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT)
      m_timingQueueSize *= 2u;

    // Maintain valid state if presentation succeeded, even if we want to
    // recreate the swapchain. Spec says that 'queue' operations, i.e. the
    // semaphore and fence signals, still happen if present fails with
    // normal swapchain errors, such as OUT_OF_DATE or SURFACE_LOST.
    if (m_hasSwapchainMaintenance1) {
      currSync.fenceSignaled = status != VK_ERROR_OUT_OF_DEVICE_MEMORY
                            && status != VK_ERROR_OUT_OF_HOST_MEMORY
                            && status != VK_ERROR_DEVICE_LOST;

      if (m_hasGamescopeFenceSignalBug)
        currSync.fenceSignaled = status >= 0;
    }

    if (status >= 0) {
      m_acquireStatus = VK_NOT_READY;

      m_frameIndex += 1;
      m_frameIndex %= m_semaphores.size();
    }

    // Add frame to waiter queue with current properties
    if (m_hasPresentWait) {
      PresenterFrame frame;
      frame.frameId = frameId;
      frame.tracker = tracker;
      frame.mode = m_presentMode;
      frame.result = status;
      frame.deadline = frameDeadline;

      pushFrame(frame);
    }

    // On a successful present, try to acquire next image already, in
    // order to hide potential delays from the application thread.
    if (status == VK_SUCCESS) {
      PresenterSync& nextSync = m_semaphores.at(m_frameIndex);
      waitForSwapchainFence(nextSync);

      m_acquireStatus = m_vkd->vkAcquireNextImageKHR(m_vkd->device(),
        m_swapchain, std::numeric_limits<uint64_t>::max(),
        nextSync.acquire, VK_NULL_HANDLE, &m_imageIndex);
    }

    // Recreate the swapchain on the next acquire, even if we get suboptimal.
    // There is no guarantee that suboptimal state is returned by both functions.
    std::lock_guard lock(m_surfaceMutex);

    if (status != VK_SUCCESS) {
      Logger::info(str::format("Presenter: Got ", status, ", recreating swapchain"));

      m_dirtySwapchain = true;
    }

    m_presentPending = false;
    m_surfaceCond.notify_one();
    return status;
  }


  void Presenter::signalFrame(
          uint64_t                frameId,
    const Rc<DxvkLatencyTracker>& tracker) {
    if (m_signal == nullptr || !frameId)
      return;

    if (m_hasPresentWait) {
      bool canSignal = false;

      { std::unique_lock lock(m_frameMutex);

        m_lastSignaled = frameId;
        canSignal = m_lastCompleted >= frameId;
      }

      if (canSignal)
        m_signal->signal(frameId);
    } else {
      m_fpsLimiter.delay();
      m_signal->signal(frameId);

      if (tracker)
        tracker->notifyGpuPresentEnd(frameId);
    }
  }


  bool Presenter::supportsColorSpace(VkColorSpaceKHR colorspace) {
    std::lock_guard lock(m_surfaceMutex);

    if (!m_surface) {
      VkResult vr = createSurface();

      if (vr != VK_SUCCESS)
        return false;
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    getSupportedFormats(surfaceFormats);

    for (const auto& surfaceFormat : surfaceFormats) {
      if (surfaceFormat.colorSpace == colorspace)
        return true;

      for (const auto& fallback : s_colorSpaceFallbacks) {
        if (fallback.first == colorspace && fallback.second == surfaceFormat.colorSpace)
          return true;
      }
    }

    return false;
  }


  void Presenter::invalidateSurface() {
    std::lock_guard lock(m_surfaceMutex);

    m_dirtySurface = true;
  }


  void Presenter::destroyResources() {
    std::unique_lock lock(m_surfaceMutex);

    m_surfaceCond.wait(lock, [this] {
      VkResult status = m_device->getDeviceStatus();
      return !m_presentPending || status < 0;
    });

    destroySwapchain();
    destroySurface();
  }


  void Presenter::setLatencySleepModeNv(
    const VkLatencySleepModeInfoNV& sleepMode) {
    std::unique_lock lock(m_surfaceMutex);

    if (sleepMode.sType != VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV)
      return;

    if (sleepMode.pNext)
      Logger::warn("Presenter: Extended sleep mode info not supported");

    // Avoid creating a swapchain with low-latency features
    // enabled if the functionality isn't required
    bool isDefault = !sleepMode.lowLatencyMode
                  && !sleepMode.lowLatencyBoost
                  && !sleepMode.minimumIntervalUs;

    if (!m_latencySleepMode && isDefault)
      return;

    m_dirtySwapchain |= !m_latencySleepMode;

    if (m_latencySleepMode) {
      m_latencySleepModeDirty |=
        m_latencySleepMode->lowLatencyMode != sleepMode.lowLatencyMode ||
        m_latencySleepMode->lowLatencyBoost != sleepMode.lowLatencyBoost ||
        m_latencySleepMode->minimumIntervalUs != sleepMode.minimumIntervalUs;
    }

    m_latencySleepMode = sleepMode;
    m_latencySleepMode->pNext = nullptr;
  }


  dxvk::high_resolution_clock::time_point Presenter::setLatencyMarkerNv(
          uint64_t                frameId,
          VkLatencyMarkerNV       marker) {
    std::unique_lock lock(m_surfaceMutex);

    if (!m_latencySleepMode) {
      // Applications may use latency markers without enabling
      // low-latency mode, make sure we have a compatible swapchain
      m_latencySleepMode = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV };
      m_dirtySwapchain = true;

      return dxvk::high_resolution_clock::now();
    }

    // Return a CPU timestamp to correlate timestamps from
    // latency frame reports with actual CPU timestamps
    auto t0 = dxvk::high_resolution_clock::now();

    if (m_latencySleepSupported) {
      VkSetLatencyMarkerInfoNV info = { VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV };
      info.presentID = frameId;
      info.marker = marker;

      m_vkd->vkSetLatencyMarkerNV(m_vkd->device(), m_swapchain, &info);
    }

    auto t1 = dxvk::high_resolution_clock::now();
    return t0 + (t1 - t0) / 2u;
  }


  dxvk::high_resolution_clock::duration Presenter::latencySleepNv() {
    std::unique_lock lock(m_surfaceMutex);

    if (!m_latencySleepSupported)
      return dxvk::high_resolution_clock::duration(0u);

    if (!m_latencySemaphore) {
      if (createLatencySemaphore() != VK_SUCCESS)
        return dxvk::high_resolution_clock::duration(0u);
    }

    VkLatencySleepInfoNV info = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV };
    info.signalSemaphore = m_latencySemaphore;
    info.value = ++m_latencySleepCounter;

    m_vkd->vkLatencySleepNV(m_vkd->device(), m_swapchain, &info);

    lock.unlock();

    auto t0 = dxvk::high_resolution_clock::now();

    VkSemaphoreWaitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &info.signalSemaphore;
    waitInfo.pValues = &info.value;

    m_vkd->vkWaitSemaphores(m_vkd->device(), &waitInfo, ~0ull);

    auto t1 = dxvk::high_resolution_clock::now();
    return t1 - t0;
  }


  uint32_t Presenter::getLatencyTimingsNv(
          uint32_t                timingCount,
          VkLatencyTimingsFrameReportNV* timings) {
    std::unique_lock lock(m_surfaceMutex);

    if (!m_latencySleepSupported)
      return 0u;

    VkGetLatencyMarkerInfoNV info = { VK_STRUCTURE_TYPE_GET_LATENCY_MARKER_INFO_NV };
    info.timingCount = timingCount;
    info.pTimings = timings;

    m_vkd->vkGetLatencyTimingsNV(m_vkd->device(), m_swapchain, &info);
    return info.timingCount;
  }


  void Presenter::setSyncInterval(uint32_t syncInterval) {
    std::lock_guard lock(m_surfaceMutex);

    // Normalize sync interval for present modes. We currently
    // cannot support anything other than 1 natively anyway.
    syncInterval = std::min(syncInterval, 1u);

    if (m_preferredSyncInterval != syncInterval) {
      m_preferredSyncInterval = syncInterval;

      if (m_dynamicModes.empty())
        m_dirtySwapchain = true;
    }
  }


  void Presenter::setFrameRateLimit(double frameRate, uint32_t maxLatency) {
    m_fpsLimiter.setTargetFrameRate(frameRate, maxLatency);

    std::lock_guard lock(m_timingMutex);

    if (m_frameRateLimit != frameRate) {
      m_frameRateLimit = frameRate;
      updateTimingMode();
    }
  }


  void Presenter::setSurfaceFormat(VkSurfaceFormatKHR format) {
    std::lock_guard lock(m_surfaceMutex);

    if (m_preferredFormat.format != format.format || m_preferredFormat.colorSpace != format.colorSpace) {
      m_preferredFormat = format;
      m_dirtySwapchain = true;
    }
  }


  void Presenter::setSurfaceExtent(VkExtent2D extent) {
    std::lock_guard lock(m_surfaceMutex);

    if (m_preferredExtent != extent) {
      m_preferredExtent = extent;
      m_dirtySwapchain = true;
    }
  }


  void Presenter::setHdrMetadata(VkHdrMetadataEXT hdrMetadata) {
    std::lock_guard lock(m_surfaceMutex);

    if (hdrMetadata.sType != VK_STRUCTURE_TYPE_HDR_METADATA_EXT) {
      m_hdrMetadata = std::nullopt;
      return;
    }

    if (hdrMetadata.pNext)
      Logger::warn("Presenter: HDR metadata extensions not currently supported.");

    m_hdrMetadata = hdrMetadata;
    m_hdrMetadata->pNext = nullptr;

    m_hdrMetadataDirty = true;
  }


  VkResult Presenter::recreateSwapChain() {
    VkResult vr;

    if (m_swapchain)
      destroySwapchain();

    if (m_surface) {
      vr = createSwapChain();

      if (vr == VK_ERROR_SURFACE_LOST_KHR)
        destroySurface();
    }

    if (!m_surface) {
      vr = createSurface();

      if (vr == VK_SUCCESS)
        vr = createSwapChain();
    }

    return vr;
  }


  void Presenter::updateSwapChain() {
    if (m_dirtySurface || m_dirtySwapchain) {
      destroySwapchain();
      m_dirtySwapchain = false;
    }

    if (m_dirtySurface) {
      destroySurface();
      m_dirtySurface = false;
    }
  }


  VkResult Presenter::createSwapChain() {
    VkSurfaceFullScreenExclusiveInfoEXT fullScreenExclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenExclusiveInfo.fullScreenExclusive = m_fullscreenMode;

    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
    surfaceInfo.surface = m_surface;

    if (m_device->features().extFullScreenExclusive)
      surfaceInfo.pNext = &fullScreenExclusiveInfo;

    // Query surface capabilities. Some properties might have changed,
    // including the size limits and supported present modes, so we'll
    // just query everything again.
    VkPresentTimingSurfaceCapabilitiesEXT presentTimingCaps = { VK_STRUCTURE_TYPE_PRESENT_TIMING_SURFACE_CAPABILITIES_EXT };
    VkSurfaceCapabilitiesPresentWait2KHR presentWait2Caps = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_WAIT_2_KHR };
    VkSurfaceCapabilitiesPresentId2KHR presentId2Caps = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_ID_2_KHR };

    VkSurfaceCapabilities2KHR caps = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };

    if (m_device->features().khrPresentId2.presentId2) {
      presentId2Caps.pNext = std::exchange(caps.pNext, &presentId2Caps);

      if (m_device->features().khrPresentWait2.presentWait2)
        presentWait2Caps.pNext = std::exchange(caps.pNext, &presentWait2Caps);

      if (m_device->features().extPresentTiming.presentTiming)
        presentTimingCaps.pNext = std::exchange(caps.pNext, &presentTimingCaps);
    }

    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;

    VkResult status;

    if (m_device->instance()->extensions().khrGetSurfaceCapabilities2.specVersion) {
      status = m_vki->vkGetPhysicalDeviceSurfaceCapabilities2KHR(
        m_device->adapter()->handle(), &surfaceInfo, &caps);
    } else {
      status = m_vki->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        m_device->adapter()->handle(), m_surface, &caps.surfaceCapabilities);
    }

    if (status) {
      Logger::err(str::format("Presenter: Failed to get surface capabilities: ", status));
      return status;
    }

    // Select image extent based on current surface capabilities, and return
    // immediately if we cannot create an actual swap chain.
    VkExtent2D imageExtent = pickImageExtent(caps.surfaceCapabilities, m_preferredExtent);

    if (!imageExtent.width || !imageExtent.height)
      return VK_NOT_READY;

    // Select format based on swap chain properties
    if ((status = getSupportedFormats(formats)))
      return status;

    VkSurfaceFormatKHR surfaceFormat = pickSurfaceFormat(formats.size(), formats.data(), m_preferredFormat);

    // Set up image format list for mutable swap chain if necessary
    small_vector<VkFormat, 2> viewFormats = { };

    auto formatPair = vk::getSrgbFormatPair(surfaceFormat.format);

    if (formatPair.second) {
      viewFormats.push_back(formatPair.first);
      viewFormats.push_back(formatPair.second);
    }

    // Select a present mode for the current sync interval
    if ((status = getSupportedPresentModes(modes)))
      return status;

    m_presentMode = pickPresentMode(modes.size(), modes.data(), m_preferredSyncInterval);

    // Check whether we can change present modes dynamically. This may
    // influence the image count as well as further swap chain creation.
    std::vector<VkPresentModeKHR> dynamicModes = {{
      pickPresentMode(modes.size(), modes.data(), 0),
      pickPresentMode(modes.size(), modes.data(), 1),
    }};

    std::vector<VkPresentModeKHR> compatibleModes;

    // As for the minimum image count, start with the most generic value
    // that works with all present modes.
    uint32_t minImageCount = caps.surfaceCapabilities.minImageCount;
    uint32_t maxImageCount = caps.surfaceCapabilities.maxImageCount;

    if (m_hasSwapchainMaintenance1) {
      VkSurfacePresentModeCompatibilityKHR compatibleModeInfo = { VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_KHR };

      VkSurfacePresentModeKHR presentModeInfo = { VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR };
      presentModeInfo.pNext = const_cast<void*>(std::exchange(surfaceInfo.pNext, &presentModeInfo));
      presentModeInfo.presentMode = m_presentMode;

      caps.pNext = &compatibleModeInfo;

      if ((status = m_vki->vkGetPhysicalDeviceSurfaceCapabilities2KHR(
          m_device->adapter()->handle(), &surfaceInfo, &caps))) {
        Logger::err(str::format("Presenter: Failed to get surface capabilities: ", status));
        return status;
      }

      compatibleModes.resize(compatibleModeInfo.presentModeCount);
      compatibleModeInfo.pPresentModes = compatibleModes.data();

      if ((status = m_vki->vkGetPhysicalDeviceSurfaceCapabilities2KHR(
          m_device->adapter()->handle(), &surfaceInfo, &caps))) {
        Logger::err(str::format("Presenter: Failed to get surface capabilities: ", status));
        return status;
      }

      // Remove modes we don't need for the purpose of finding the minimum
      // image count, as well as for swap chain creation later.
      compatibleModes.erase(std::remove_if(compatibleModes.begin(), compatibleModes.end(),
        [&dynamicModes] (VkPresentModeKHR mode) {
          return std::find(dynamicModes.begin(), dynamicModes.end(), mode) == dynamicModes.end();
        }), compatibleModes.end());

      minImageCount = 0;
      caps.pNext = nullptr;

      for (auto mode : compatibleModes) {
        presentModeInfo.presentMode = mode;

        if ((status = m_vki->vkGetPhysicalDeviceSurfaceCapabilities2KHR(
            m_device->adapter()->handle(), &surfaceInfo, &caps))) {
          Logger::err(str::format("Presenter: Failed to get surface capabilities: ", status));
          return status;
        }

        minImageCount = std::max(minImageCount, caps.surfaceCapabilities.minImageCount);

        if (caps.surfaceCapabilities.maxImageCount) {
          maxImageCount = maxImageCount
            ? std::min(maxImageCount, caps.surfaceCapabilities.maxImageCount)
            : caps.surfaceCapabilities.maxImageCount;
        }
      }

      // If any required mode is not supported for dynamic present
      // mode switching, clear the dynamic mode array.
      for (auto mode : dynamicModes) {
        if (std::find(compatibleModes.begin(), compatibleModes.end(), mode) == compatibleModes.end()) {
          dynamicModes.clear();
          break;
        }
      }
    } else if (dynamicModes[0] != dynamicModes[1]) {
      // If we can't switch modes dynamically, clear the
      // array so that setSyncInterval errors out properly.
      dynamicModes.clear();
    }

    // If present timing is supported, also check if there is a useful present
    // stage we can use. Prioritize the latest available stage for absolute
    // timing since we need to correlate with timing reports.
    // Also, opt out of timing if we have issues keeping the report queue to
    // a reasonable size.
    if (presentTimingCaps.presentTimingSupported && presentId2Caps.presentId2Supported
     && presentWait2Caps.presentWait2Supported && m_timingQueueSize <= MaxFrameQueueSize) {
      small_vector<VkPresentStageFlagBitsEXT, 3> stages;

      if (presentTimingCaps.presentAtAbsoluteTimeSupported) {
        stages.push_back(VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT);
        stages.push_back(VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT);
      } else {
        stages.push_back(VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_OUT_BIT_EXT);
        stages.push_back(VK_PRESENT_STAGE_IMAGE_FIRST_PIXEL_VISIBLE_BIT_EXT);
      }

      stages.push_back(VK_PRESENT_STAGE_REQUEST_DEQUEUED_BIT_EXT);

      for (auto stage : stages) {
        if (presentTimingCaps.presentStageQueries & stage) {
          m_timingMode.presentStage = stage;
          break;
        }
      }

      if (m_timingMode.presentStage) {
        m_timingMode.supportsRelative = presentTimingCaps.presentAtRelativeTimeSupported;
        m_timingMode.supportsAbsolute = presentTimingCaps.presentAtAbsoluteTimeSupported;
      }
    }

    // Compute swap chain image count based on available info
    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = m_fullscreenMode;

    VkSwapchainPresentModesCreateInfoKHR modeInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR };
    modeInfo.presentModeCount       = compatibleModes.size();
    modeInfo.pPresentModes          = compatibleModes.data();

    VkSwapchainLatencyCreateInfoNV latencyInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_LATENCY_CREATE_INFO_NV };
    latencyInfo.latencyModeEnable   = m_latencySleepMode.has_value();

    VkImageFormatListCreateInfo formatList = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };
    formatList.viewFormatCount      = viewFormats.size();
    formatList.pViewFormats         = viewFormats.data();

    VkSwapchainCreateInfoKHR swapInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapInfo.surface                = m_surface;
    swapInfo.minImageCount          = pickImageCount(minImageCount, maxImageCount);
    swapInfo.imageFormat            = surfaceFormat.format;
    swapInfo.imageColorSpace        = surfaceFormat.colorSpace;
    swapInfo.imageExtent            = imageExtent;
    swapInfo.imageArrayLayers       = 1;
    swapInfo.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapInfo.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform           = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapInfo.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode            = m_presentMode;
    swapInfo.clipped                = VK_TRUE;

    if (m_device->features().khrSwapchainMutableFormat && formatList.viewFormatCount) {
      swapInfo.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;
      formatList.pNext = std::exchange(swapInfo.pNext, &formatList);
    }

    if (presentId2Caps.presentId2Supported)
      swapInfo.flags |= VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR;

    if (presentWait2Caps.presentWait2Supported)
      swapInfo.flags |= VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR;

    if (m_timingMode.presentStage)
      swapInfo.flags |= VK_SWAPCHAIN_CREATE_PRESENT_TIMING_BIT_EXT;

    if (m_device->features().extFullScreenExclusive)
      fullScreenInfo.pNext = const_cast<void*>(std::exchange(swapInfo.pNext, &fullScreenInfo));

    if (m_hasSwapchainMaintenance1)
      modeInfo.pNext = std::exchange(swapInfo.pNext, &modeInfo);

    if (m_device->features().nvLowLatency2)
      latencyInfo.pNext = std::exchange(swapInfo.pNext, &latencyInfo);

    Logger::info(str::format(
      "Presenter: Actual swapchain properties:"
      "\n  Format:          ", swapInfo.imageFormat,
      "\n  Color space:     ", swapInfo.imageColorSpace,
      "\n  Present mode:    ", swapInfo.presentMode, " (dynamic: ", (dynamicModes.empty() ? "no)" : "yes)"),
      "\n  Buffer size:     ", swapInfo.imageExtent.width, "x", swapInfo.imageExtent.height,
      "\n  Image count:     ", swapInfo.minImageCount,
      "\n  Absolute timimg: ", m_timingMode.supportsAbsolute ? "yes" : "no",
      "\n  Relative timimg: ", m_timingMode.supportsRelative ? "yes" : "no"));

    if ((status = m_vkd->vkCreateSwapchainKHR(m_vkd->device(), &swapInfo, nullptr, &m_swapchain))) {
      Logger::err(str::format("Presenter: Failed to create Vulkan swapchain: ", status));
      return status;
    }
    
    // Import actual swap chain images
    std::vector<VkImage> images;

    if ((status = getSwapImages(images)))
      return status;
    
    for (uint32_t i = 0; i < images.size(); i++) {
      std::string debugName = str::format("Vulkan swap image ", i);

      DxvkImageCreateInfo imageInfo = { };
      imageInfo.type        = VK_IMAGE_TYPE_2D;
      imageInfo.format      = swapInfo.imageFormat;
      imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.extent      = { swapInfo.imageExtent.width, swapInfo.imageExtent.height, 1u };
      imageInfo.numLayers   = swapInfo.imageArrayLayers;
      imageInfo.mipLevels   = 1u;
      imageInfo.usage       = swapInfo.imageUsage;
      imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.layout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      imageInfo.colorSpace  = swapInfo.imageColorSpace;
      imageInfo.shared      = VK_TRUE;
      imageInfo.debugName   = debugName.c_str();

      // If possible, expose the image with an sRGB format internally so
      // that it will be used as the default format for composition.
      if (swapInfo.flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR) {
        imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        imageInfo.viewFormatCount = formatList.viewFormatCount;
        imageInfo.viewFormats = formatList.pViewFormats;

        if (formatPair.second)
          imageInfo.format = formatPair.second;
      }

      m_images.push_back(m_device->importImage(imageInfo, images[i],
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    }

    // Create one set of semaphores per swap image, as well as a fence
    // that we use to ensure that semaphores are safe to access.
    uint32_t semaphoreCount = images.size();

    if (!m_hasSwapchainMaintenance1) {
      // Without support for present fences, just give up and allocate extra
      // semaphores. We have no real guarantees when they are safe to access.
      semaphoreCount *= 2u;
    }

    m_semaphores.resize(semaphoreCount);

    for (uint32_t i = 0; i < semaphoreCount; i++) {
      VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

      if ((status = m_vkd->vkCreateSemaphore(m_vkd->device(),
          &semInfo, nullptr, &m_semaphores[i].acquire))) {
        Logger::err(str::format("Presenter: Failed to create semaphore: ", status));
        return status;
      }

      if ((status = m_vkd->vkCreateSemaphore(m_vkd->device(),
          &semInfo, nullptr, &m_semaphores[i].present))) {
        Logger::err(str::format("Presenter: Failed to create semaphore: ", status));
        return status;
      }

      VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

      if ((status = m_vkd->vkCreateFence(m_vkd->device(),
          &fenceInfo, nullptr, &m_semaphores[i].fence))) {
        Logger::err(str::format("Presenter: Failed to create fence: ", status));
        return status;
      }
    }
    
    // Invalidate indices
    m_latencySleepSupported = m_device->features().nvLowLatency2 && latencyInfo.latencyModeEnable;

    m_imageIndex = 0;
    m_frameIndex = 0;

    m_dynamicModes = std::move(dynamicModes);

    // Set up feature support for present wait / id, and launch sync thread as necessary
    m_hasPresentId = presentId2Caps.presentId2Supported || m_device->features().khrPresentId.presentId;
    m_hasPresentWait = presentWait2Caps.presentWait2Supported || m_device->features().khrPresentWait.presentWait;

    if (m_signal && m_hasPresentWait && !m_frameThread.joinable())
      m_frameThread = dxvk::thread([this] { runFrameThread(); });

    // Set up initial present timing state
    if (m_timingMode.presentStage) {
      m_vkd->vkSetSwapchainPresentTimingQueueSizeEXT(m_vkd->device(), m_swapchain, m_timingQueueSize);

      updateDisplayTiming();

      updateTimingDomains();
      recalibrateTimeDomains();

      updateTimingMode();
    }

    return VK_SUCCESS;
  }


  VkResult Presenter::getSupportedFormats(std::vector<VkSurfaceFormatKHR>& formats) const {
    uint32_t numFormats = 0;

    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = m_fullscreenMode;

    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, &fullScreenInfo };
    surfaceInfo.surface = m_surface;

    VkResult status;
    
    if (m_device->features().extFullScreenExclusive) {
      status = m_vki->vkGetPhysicalDeviceSurfaceFormats2KHR(
        m_device->adapter()->handle(), &surfaceInfo, &numFormats, nullptr);
    } else {
      status = m_vki->vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_device->adapter()->handle(), m_surface, &numFormats, nullptr);
    }

    if (status != VK_SUCCESS) {
      Logger::err(str::format("Presenter: Failed to query surface formats: ", status));
      return status;
    }
    
    formats.resize(numFormats);

    if (m_device->features().extFullScreenExclusive) {
      std::vector<VkSurfaceFormat2KHR> tmpFormats(numFormats, 
        { VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR, nullptr, VkSurfaceFormatKHR() });

      status = m_vki->vkGetPhysicalDeviceSurfaceFormats2KHR(
        m_device->adapter()->handle(), &surfaceInfo, &numFormats, tmpFormats.data());

      for (uint32_t i = 0; i < numFormats; i++)
        formats[i] = tmpFormats[i].surfaceFormat;
    } else {
      status = m_vki->vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_device->adapter()->handle(), m_surface, &numFormats, formats.data());
    }

    if (status != VK_SUCCESS)
      Logger::err(str::format("Presenter: Failed to query surface formats: ", status));

    return status;
  }

  
  VkResult Presenter::getSupportedPresentModes(std::vector<VkPresentModeKHR>& modes) const {
    uint32_t numModes = 0;

    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = m_fullscreenMode;

    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, &fullScreenInfo };
    surfaceInfo.surface = m_surface;

    VkResult status;

    if (m_device->features().extFullScreenExclusive) {
      status = m_vki->vkGetPhysicalDeviceSurfacePresentModes2EXT(
        m_device->adapter()->handle(), &surfaceInfo, &numModes, nullptr);
    } else {
      status = m_vki->vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device->adapter()->handle(), m_surface, &numModes, nullptr);
    }

    if (status != VK_SUCCESS) {
      Logger::err(str::format("Presenter: Failed to query present modes: ", status));
      return status;
    }
    
    modes.resize(numModes);

    if (m_device->features().extFullScreenExclusive) {
      status = m_vki->vkGetPhysicalDeviceSurfacePresentModes2EXT(
        m_device->adapter()->handle(), &surfaceInfo, &numModes, modes.data());
    } else {
      status = m_vki->vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device->adapter()->handle(), m_surface, &numModes, modes.data());
    }

    if (status != VK_SUCCESS)
      Logger::err(str::format("Presenter: Failed to query present modes: ", status));

    return status;
  }


  VkResult Presenter::getSwapImages(std::vector<VkImage>& images) {
    uint32_t imageCount = 0;

    VkResult status = m_vkd->vkGetSwapchainImagesKHR(
      m_vkd->device(), m_swapchain, &imageCount, nullptr);
    
    if (status != VK_SUCCESS) {
      Logger::err(str::format("Presenter: Failed to query swapchain images: ", status));
      return status;
    }
    
    images.resize(imageCount);

    status = m_vkd->vkGetSwapchainImagesKHR(
      m_vkd->device(), m_swapchain, &imageCount, images.data());

    if (status != VK_SUCCESS)
      Logger::err(str::format("Presenter: Failed to query swapchain images: ", status));

    return status;
  }


  VkSurfaceFormatKHR Presenter::pickSurfaceFormat(
          uint32_t                  numSupported,
    const VkSurfaceFormatKHR*       pSupported,
    const VkSurfaceFormatKHR&       desired) {
    VkSurfaceFormatKHR result = { };
    result.colorSpace = pickColorSpace(numSupported, pSupported, desired.colorSpace);
    result.format = pickFormat(numSupported, pSupported, result.colorSpace,
      result.colorSpace == desired.colorSpace ? desired.format : VK_FORMAT_UNDEFINED);
    return result;
  }


  VkColorSpaceKHR Presenter::pickColorSpace(
          uint32_t                  numSupported,
    const VkSurfaceFormatKHR*       pSupported,
          VkColorSpaceKHR           desired) {
    VkColorSpaceKHR fallback = pSupported[0].colorSpace;

    for (uint32_t i = 0; i < numSupported; i++) {
      if (pSupported[i].colorSpace == desired)
        return desired;

      if (pSupported[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        fallback = pSupported[i].colorSpace;
    }

    for (const auto& f : s_colorSpaceFallbacks) {
      if (f.first != desired)
        continue;

      for (uint32_t i = 0; i < numSupported; i++) {
        if (pSupported[i].colorSpace == f.second)
          return f.second;
      }
    }

    Logger::warn(str::format("No fallback color space found for ", desired, ", using ", fallback));
    return fallback;
  }


  VkFormat Presenter::pickFormat(
          uint32_t                  numSupported,
    const VkSurfaceFormatKHR*       pSupported,
          VkColorSpaceKHR           colorSpace,
          VkFormat                  format) {
    static const std::array<VkFormat, 13> srgbFormatList = {
      VK_FORMAT_B5G5R5A1_UNORM_PACK16,
      VK_FORMAT_R5G5B5A1_UNORM_PACK16,
      VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,
      VK_FORMAT_R5G6B5_UNORM_PACK16,
      VK_FORMAT_B5G6R5_UNORM_PACK16,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_A8B8G8R8_UNORM_PACK32,
      VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
      VK_FORMAT_R16G16B16A16_UNORM,
      VK_FORMAT_R16G16B16A16_SFLOAT,
    };

    static const std::array<VkFormat, 5> hdr10FormatList = {
      VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
      VK_FORMAT_R16G16B16A16_UNORM,
      VK_FORMAT_R16G16B16A16_SFLOAT,
    };

    static const std::array<VkFormat, 1> scRGBFormatList = {
      VK_FORMAT_R16G16B16A16_SFLOAT,
    };

    static const std::array<PresenterFormatList, 3> compatLists = {{
      { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        srgbFormatList.size(), srgbFormatList.data() },
      { VK_COLOR_SPACE_HDR10_ST2084_EXT,
        hdr10FormatList.size(), hdr10FormatList.data() },
      { VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
        scRGBFormatList.size(), scRGBFormatList.data() },
    }};

    // Some third-party overlays don't handle sRGB image formats
    // correctly, so use the corresponding UNORM format instead.
    auto formatPair = vk::getSrgbFormatPair(format);

    if (formatPair.first)
      format = formatPair.first;

    // If the desired format is supported natively, use it
    VkFormat fallback = VK_FORMAT_UNDEFINED;

    for (uint32_t i = 0; i < numSupported; i++) {
      if (pSupported[i].colorSpace == colorSpace) {
        if (pSupported[i].format == format)
          return pSupported[i].format;

        if (!fallback)
          fallback = pSupported[i].format;
      }
    }

    // Otherwise, find a supported format for the color space
    const PresenterFormatList* compatList = nullptr;

    for (const auto& l : compatLists) {
      if (l.colorSpace == colorSpace)
        compatList = &l;
    }

    if (!compatList)
      return fallback;

    bool desiredFound = false;

    for (uint32_t i = 0; i < compatList->formatCount; i++) {
      bool isSupported = false;

      if (compatList->formats[i] == format)
        desiredFound = true;

      for (uint32_t j = 0; j < numSupported && !isSupported; j++)
        isSupported = pSupported[j].colorSpace == colorSpace && pSupported[j].format == compatList->formats[i];

      if (isSupported) {
        fallback = compatList->formats[i];

        if (desiredFound)
          break;
      }
    }

    if (!desiredFound && format)
      Logger::warn(str::format("Desired format ", format, " not in compatibility list for ", colorSpace, ", using ", fallback));

    return fallback;
  }


  VkPresentModeKHR Presenter::pickPresentMode(
          uint32_t                  numSupported,
    const VkPresentModeKHR*         pSupported,
          uint32_t                  syncInterval) {
    std::array<VkPresentModeKHR, 2> desired = { };
    uint32_t numDesired = 0;

    Tristate tearFree = m_device->config().tearFree;

    if (!syncInterval) {
      if (tearFree != Tristate::True)
        desired[numDesired++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
      desired[numDesired++] = VK_PRESENT_MODE_MAILBOX_KHR;
    } else {
      if (tearFree == Tristate::False)
        desired[numDesired++] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    // Just pick the first desired and supported mode
    for (uint32_t i = 0; i < numDesired; i++) {
      for (uint32_t j = 0; j < numSupported; j++) {
        if (pSupported[j] == desired[i])
          return pSupported[j];
      }
    }
    
    // Guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
  }


  VkExtent2D Presenter::pickImageExtent(
    const VkSurfaceCapabilitiesKHR& caps,
          VkExtent2D                desired) {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
      return caps.currentExtent;

    VkExtent2D actual;
    actual.width  = clamp(desired.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    actual.height = clamp(desired.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return actual;
  }


  uint32_t Presenter::pickImageCount(
          uint32_t                  minImageCount,
          uint32_t                  maxImageCount) {
    uint32_t count = minImageCount + 1;

    if (count > maxImageCount && maxImageCount != 0)
      count = maxImageCount;

    return count;
  }


  void Presenter::updateTimingDomains() {
    // Reset domain info in case there's an error
    m_timingDomains = std::nullopt;

    uint64_t updateCounter = 0u;

    small_vector<VkTimeDomainKHR, 16u> domains;
    small_vector<uint64_t, 16u> domainIds;

    VkResult status = VK_INCOMPLETE;

    while (status == VK_INCOMPLETE) {
      VkSwapchainTimeDomainPropertiesEXT domainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT };

      status = m_vkd->vkGetSwapchainTimeDomainPropertiesEXT(
        m_vkd->device(), m_swapchain, &domainInfo, nullptr);

      if (status != VK_SUCCESS)
        break;

      domains.resize(domainInfo.timeDomainCount);
      domainIds.resize(domainInfo.timeDomainCount);

      // Fetch update counter now. If the internal counter increases between
      // the first and the second call, we may end up with a different number
      // of available domains, so handle VK_INCOMPLETE returns here and resize
      // the arrays appropriately.
      domainInfo.pTimeDomains = domains.data();
      domainInfo.pTimeDomainIds = domainIds.data();

      status = m_vkd->vkGetSwapchainTimeDomainPropertiesEXT(
        m_vkd->device(), m_swapchain, &domainInfo, &updateCounter);

      domains.resize(domainInfo.timeDomainCount);
      domainIds.resize(domainInfo.timeDomainCount);
    }

    if (status != VK_SUCCESS) {
      Logger::warn(str::format("Presenter: Failed to query swapchain time domains: ", status));
      return;
    }

    // We're guaranteed at least one PRESENT_STAGE_LOCAL domain. Just
    // use that for actual timing purposes and ignore everything else.
    // We will add report time domains on demand.
    auto& info = m_timingDomains.emplace();
    info.updateCounter = updateCounter;

    for (size_t i = 0u; i < domains.size(); i++) {
      if (domains[i] == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT) {
        auto& domain = info.domains.emplace_back();
        domain.timeDomain = domains[i];
        domain.timeDomainId = domainIds[i];

        m_timingMode.timeDomainId = domainIds[i];
      }
    }

    // Check if there is a QPC domain and add it as necessary.
    uint32_t timeDomainCount = 0u;

    status = m_vki->vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(
      m_device->adapter()->handle(), &timeDomainCount, nullptr);

    if (status == VK_SUCCESS) {
      domains.resize(timeDomainCount);

      status = m_vki->vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(
        m_device->adapter()->handle(), &timeDomainCount, domains.data());
    }

    if (status) {
      Logger::warn(str::format("Presenter: Failed to query time domains: ", status));
      return;
    }

    for (size_t i = 0u; i < domains.size(); i++) {
      if (domains[i] == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR) {
        auto& domain = info.domains.emplace_back();
        domain.timeDomain = domains[i];
      }
    }
  }


  void Presenter::updateDisplayTiming() {
    // Reset state in case of an error
    m_timingDisplayInfo = std::nullopt;

    uint64_t updateCounter = 0u;

    VkSwapchainTimingPropertiesEXT swapchainTiming = { VK_STRUCTURE_TYPE_SWAPCHAIN_TIMING_PROPERTIES_EXT };
    VkResult status = m_vkd->vkGetSwapchainTimingPropertiesEXT(
      m_vkd->device(), m_swapchain, &swapchainTiming, &updateCounter);

    if (status) {
      Logger::warn(str::format("Presenter: Failed to query display timing: ", status));
      return;
    }

    auto& info = m_timingDisplayInfo.emplace();
    info.updateCounter = updateCounter;

    // This can happen on some platforms. We can't meaningfully enable timing.
    if (!swapchainTiming.refreshDuration && !swapchainTiming.refreshInterval) {
      Logger::warn(str::format("Presenter: Unable to determine display refresh rate"));
      return;
    }

    // Swapchain timing is all sorts of weird, values of 0 generally indicate that
    // the corresponding property is unknown, and UINT64_MAX for refreshInterval
    // implies variable refresh rate.
    info.isVariableRefresh = swapchainTiming.refreshInterval == uint64_t(-1);
    info.refreshIntervalNs = swapchainTiming.refreshDuration;

    if (!info.refreshIntervalNs && !info.isVariableRefresh)
      info.refreshIntervalNs = swapchainTiming.refreshInterval;
  }


  void Presenter::updateTimingMode() {
    m_timingMode.relativeTiming = false;
    m_timingMode.absoluteTiming = false;

    // Can't meaningfully do timing if we don't know about display timing
    if (!m_timingDomains || !m_timingDisplayInfo)
      return;

    // Compute target frame interval from frame rate limit
    m_timingMode.frameIntervalNs = m_frameRateLimit != 0.0
      ? uint64_t(1000000000.0 / std::abs(m_frameRateLimit))
      : uint64_t(0u);

    if (!m_timingMode.presentStage || m_timingMode.frameIntervalNs <= m_timingDisplayInfo->refreshIntervalNs) {
      Logger::info("Presenter: Present timing disabled");
      return;
    }

    // Probe relative timing first since that is most likely to give us
    // consistent pacing, without any setup work required from our side.
    if (m_timingMode.supportsRelative) {
      if (m_timingDisplayInfo->isVariableRefresh) {
        // Always enable relative timing for VRR
        m_timingMode.relativeTiming = true;
      } else if (m_timingDisplayInfo->refreshIntervalNs) {
        // Otherwise, check if the frame duration is reasonably close
        // to a multiple of the display refresh rate.
        uint64_t maxDeltaNs = m_timingMode.frameIntervalNs / 100u;
        uint64_t realDeltaNs = (m_timingMode.frameIntervalNs + maxDeltaNs) % m_timingDisplayInfo->refreshIntervalNs;

        m_timingMode.relativeTiming = realDeltaNs <= 2u * maxDeltaNs;

        if (m_timingMode.relativeTiming)
          m_timingMode.frameIntervalNs = (m_timingMode.frameIntervalNs + maxDeltaNs) - realDeltaNs;
      }
    }

    // Fall back to absolute timing if we cannot use relative timimg.
    if (m_timingMode.supportsAbsolute)
      m_timingMode.absoluteTiming = !m_timingMode.relativeTiming;

    // Reset reference time and frame ID for absolute timing
    // so that we don't end up submitting bogus timestamps.
    m_timingMode.referenceTime = 0u;
    m_timingMode.referenceFrameId = 0u;

    if (m_timingMode.relativeTiming)
      Logger::info("Presenter: Present timing enabled for FIFO modes (relative)");
    else if (m_timingMode.absoluteTiming)
      Logger::info("Presenter: Present timing enabled for FIFO modes (absolute)");
    else
      Logger::info("Presenter: Present timing disabled (absolute timing unsupported)");
  }


  void Presenter::recalibrateTimeDomains() {
    if (!m_timingDomains)
      return;

    // Calibrate all known time domains at once
    auto& domains = m_timingDomains->domains;

    small_vector<VkSwapchainCalibratedTimestampInfoEXT, 16u> swapchainInfo(domains.size(),
      VkSwapchainCalibratedTimestampInfoEXT { VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT });
    small_vector<VkCalibratedTimestampInfoKHR, 16u> calibrationInfo(domains.size(),
      VkCalibratedTimestampInfoKHR { VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR });
    small_vector<uint64_t, 16u> timestamps(domains.size());

    for (size_t i = 0u; i < domains.size(); i++) {
      calibrationInfo[i].timeDomain = domains[i].timeDomain;

      if (domains[i].timeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT
       || domains[i].timeDomain == VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT) {
        swapchainInfo[i].swapchain = m_swapchain;
        swapchainInfo[i].timeDomainId = domains[i].timeDomainId;

        if (domains[i].timeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT)
          swapchainInfo[i].presentStage = m_timingMode.presentStage;

        calibrationInfo[i].pNext = &swapchainInfo[i];
      }
    }

    // Retry calibration until we get reasonable precision (150us)
    constexpr uint32_t MaxAttempts = 5u;
    constexpr uint64_t MaxDeviation = 1500000u;

    uint64_t maxDeviation = 0u;

    for (uint32_t i = 0u; i < MaxAttempts; i++) {
      VkResult status = m_vkd->vkGetCalibratedTimestampsKHR(m_vkd->device(),
        domains.size(), calibrationInfo.data(), timestamps.data(), &maxDeviation);

      if (status && !i) {
        Logger::warn(str::format("Presenter: Failed to calibrate timestamps: ", status));
        return;
      }

      if (status || maxDeviation <= MaxDeviation)
        break;
    }

    for (size_t i = 0u; i < domains.size(); i++)
      domains[i].referenceTime = timestamps[i];

    // Remember when we last calibrated everything so that we can periodically
    // re-query. Clock drift is a real issue on certain hardware configurations.
    m_timingDomains->lastCalibration = dxvk::high_resolution_clock::now();
  }


  bool Presenter::updatePresentTiming() {
    if (!m_timingMode.presentStage)
      return false;

    // Need to access both timing stuff and the frame queue here
    std::lock_guard lock(m_timingMutex);

    // Still need to drain the queue even if everything is messed up
    small_vector<VkPresentStageTimeEXT, FrameQueueSize> stageTimes;
    small_vector<VkPastPresentationTimingEXT, FrameQueueSize> reports;

    stageTimes.resize(m_timingQueueSize);
    reports.resize(m_timingQueueSize);

    for (size_t i = 0u; i < m_timingQueueSize; i++) {
      reports[i].sType = VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_EXT;
      reports[i].presentStageCount = 1u;
      reports[i].pPresentStages = &stageTimes[i];
    }

    VkPastPresentationTimingInfoEXT timingInfo = { VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_INFO_EXT };
    timingInfo.swapchain = m_swapchain;

    VkPastPresentationTimingPropertiesEXT timingProperties = { VK_STRUCTURE_TYPE_PAST_PRESENTATION_TIMING_PROPERTIES_EXT };
    timingProperties.presentationTimingCount = reports.size();
    timingProperties.pPresentationTimings = reports.data();

    VkResult status = m_vkd->vkGetPastPresentationTimingEXT(m_vkd->device(),
      &timingInfo, &timingProperties);

    if (status) {
      Logger::warn(str::format("Presenter: Failed to query past present timings: ", status));
      return false;
    }

    // Update display timing and time domains as necessary
    bool updateMode = false;
    if (!m_timingDisplayInfo || m_timingDisplayInfo->updateCounter != timingProperties.timingPropertiesCounter) {
      updateMode = true;
      updateDisplayTiming();
    }

    if (!m_timingDomains || m_timingDomains->updateCounter != timingProperties.timeDomainsCounter) {
      updateMode = true;
      updateTimingDomains();

      recalibrateTimeDomains();
    }

    if (updateMode)
      updateTimingMode();

    // Find latest available report and update present statistics. If we run
    // absolute timing and any given frame missed its deadline, restart the
    // sequence.
    std::lock_guard frameLock(m_frameMutex);
    bool hasMissedDeadline = false;

    for (size_t i = 0u; i < timingProperties.presentationTimingCount; i++) {
      const auto& time = stageTimes[i];
      const auto& report = reports[i];

      if (!report.reportComplete || !time.time || time.stage != m_timingMode.presentStage)
        continue;

      uint64_t reportTimeLocal = translateTimestamp(
        report.timeDomain, report.timeDomainId, time.time,
        VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT, m_timingMode.timeDomainId);

      uint64_t reportTimeQpc = 0u;

      if (hasQpcDomain()) {
        reportTimeQpc = translateTimestamp(
          report.timeDomain, report.timeDomainId, time.time,
          VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, 0u);
      }

      if (reportTimeLocal) {
        m_timingMode.lastFrameId = report.presentId;
        m_timingMode.lastFrameTimeLocal = reportTimeLocal;
        m_timingMode.lastFrameTimeQpc = reportTimeQpc;

        for (const auto& frame : m_frameQueue) {
          if (frame.frameId == report.presentId)
            hasMissedDeadline = reportTimeLocal > frame.deadline;
        }
      }
    }

    if (!m_timingMode.referenceFrameId || hasMissedDeadline) {
      m_timingMode.referenceFrameId = m_timingMode.lastFrameId;
      m_timingMode.referenceTime = m_timingMode.lastFrameTimeLocal;
    }

    return true;
  }


  bool Presenter::hasQpcDomain() {
    if (!m_timingDomains)
      return false;

    for (const auto& domain : m_timingDomains->domains) {
      if (domain.timeDomain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR)
        return true;
    }

    return false;
  }


  uint64_t Presenter::translateTimestamp(
          VkTimeDomainKHR           srcTimeDomain,
          uint64_t                  srcTimeDomainId,
          uint64_t                  srcTimestamp,
          VkTimeDomainKHR           dstTimeDomain,
          uint64_t                  dstTimeDomainId) {
    // Don't need to do anything if the time domains already match anyway
    if (srcTimeDomain == dstTimeDomain && srcTimeDomainId == dstTimeDomainId)
      return srcTimestamp;

    // Can't do anything if we can't calibrate time stamps
    if (!m_timingDomains)
      return 0u;

    // Determine tick frequency for QPC timestamps. Our internal high resolution
    // clock is QPC on Windows, so this should work whenever the time domain is
    // actually present.
    static std::atomic<int64_t> s_qpcFreq = { 0 };
    int64_t qpcFreq = s_qpcFreq.load(std::memory_order_relaxed);

    if (unlikely(!qpcFreq)) {
      qpcFreq = dxvk::high_resolution_clock::get_frequency();
      s_qpcFreq.store(qpcFreq, std::memory_order_relaxed);
    }

    // If the last calibration is older than a second, recalibrate.
    uint64_t calibrationAgeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
      dxvk::high_resolution_clock::now() - m_timingDomains->lastCalibration).count();

    if (calibrationAgeNs > 1000000000ull)
      recalibrateTimeDomains();

    // Check whether the source and destination time domains are known
    // and compute the delta in terms of nanoseconds or QPC ticks.
    uint64_t refTimeSrcDomain = 0u;
    uint64_t refTimeDstDomain = 0u;

    for (size_t i = 0u; i < m_timingDomains->domains.size(); i++) {
      if (m_timingDomains->domains[i].timeDomain == srcTimeDomain
       && m_timingDomains->domains[i].timeDomainId == srcTimeDomainId)
        refTimeSrcDomain = m_timingDomains->domains[i].referenceTime;

      if (m_timingDomains->domains[i].timeDomain == dstTimeDomain
       && m_timingDomains->domains[i].timeDomainId == dstTimeDomainId)
        refTimeDstDomain = m_timingDomains->domains[i].referenceTime;
    }

    // If we couldn't find one of the time domains, try to calibrate
    if (!refTimeSrcDomain || !refTimeDstDomain) {
      std::array<VkSwapchainCalibratedTimestampInfoEXT, 2u> swapchainInfo = {{
        VkSwapchainCalibratedTimestampInfoEXT { VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT },
        VkSwapchainCalibratedTimestampInfoEXT { VK_STRUCTURE_TYPE_SWAPCHAIN_CALIBRATED_TIMESTAMP_INFO_EXT },
      }};

      std::array<VkCalibratedTimestampInfoKHR, 2u> calibrationInfo = {{
        VkCalibratedTimestampInfoKHR { VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR },
        VkCalibratedTimestampInfoKHR { VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR },
      }};

      calibrationInfo[0].timeDomain = srcTimeDomain;
      calibrationInfo[1].timeDomain = dstTimeDomain;

      if (srcTimeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT || srcTimeDomain == VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT) {
        calibrationInfo[0].pNext = &swapchainInfo[0];

        swapchainInfo[0].swapchain = m_swapchain;
        swapchainInfo[0].presentStage = m_timingMode.presentStage;
        swapchainInfo[0].timeDomainId = srcTimeDomainId;
      }

      if (dstTimeDomain == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT || dstTimeDomain == VK_TIME_DOMAIN_SWAPCHAIN_LOCAL_EXT) {
        calibrationInfo[1].pNext = &swapchainInfo[1];

        swapchainInfo[1].swapchain = m_swapchain;
        swapchainInfo[1].presentStage = m_timingMode.presentStage;
        swapchainInfo[1].timeDomainId = dstTimeDomainId;
      }

      // Try to get reference timestamps for the two domains
      std::array<uint64_t, 2u> timestamps = { };

      uint64_t maxDeviation = 0u;

      VkResult status = m_vkd->vkGetCalibratedTimestampsKHR(m_vkd->device(),
        calibrationInfo.size(), calibrationInfo.data(), timestamps.data(), &maxDeviation);

      if (status) {
        Logger::err(str::format("Presenter: Failed to map timestamp from ",
          srcTimeDomain, "@", srcTimeDomainId," to domain ",
          dstTimeDomain, "@", dstTimeDomainId, ": ", status));
        return 0u;
      }

      // On success, add the domains to the domain list as necessary
      // and recalibrate all known domains for subsequent steps
      if (!refTimeSrcDomain) {
        auto& domain = m_timingDomains->domains.emplace_back();
        domain.timeDomain = srcTimeDomain;
        domain.timeDomainId = srcTimeDomainId;
      }

      if (!refTimeDstDomain) {
        auto& domain = m_timingDomains->domains.emplace_back();
        domain.timeDomain = dstTimeDomain;
        domain.timeDomainId = dstTimeDomainId;
      }

      refTimeSrcDomain = timestamps[0];
      refTimeDstDomain = timestamps[1];

      recalibrateTimeDomains();
    }

    // Compute time delta in terms of the destination time domain
    int64_t timeDelta = int64_t(srcTimestamp - refTimeSrcDomain);

    if (srcTimeDomain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR) {
      timeDelta *= 1000000000;
      timeDelta /= qpcFreq;
    }

    if (dstTimeDomain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR) {
      timeDelta *= qpcFreq;
      timeDelta /= 1000000000;
    }

    return uint64_t(refTimeDstDomain + timeDelta);
  }


  VkResult Presenter::createSurface() {
    VkResult vr = m_surfaceProc(&m_surface);

    if (vr != VK_SUCCESS)
      Logger::err(str::format("Presenter: Failed to create Vulkan surface: ", vr));

    return vr;
  }


  VkResult Presenter::createLatencySemaphore() {
    VkSemaphoreTypeCreateInfo typeInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;

    VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &typeInfo };
    VkResult vr = m_vkd->vkCreateSemaphore(m_vkd->device(), &info, nullptr, &m_latencySemaphore);

    if (vr != VK_SUCCESS)
      Logger::err(str::format("Presenter: Failed to create latency semaphore: ", vr));

    return vr;
  }


  void Presenter::destroySwapchain() {
    // Without present fence support, waiting for the queue or device to go idle
    // is the only way to properly synchronize swapchain teardown. Care must be
    // taken not to call this method while the submission queue is locked.
    if (!m_hasSwapchainMaintenance1)
      m_device->waitForIdle();

    // Wait for the presentWait worker to finish using
    // the swapchain before destroying it.
    std::unique_lock lock(m_frameMutex);

    m_frameDrain.wait(lock, [this] {
      return m_frameQueuePopId == m_frameQueuePushId;
    });

    for (auto& sem : m_semaphores)
      waitForSwapchainFence(sem);

    for (const auto& sem : m_semaphores) {
      m_vkd->vkDestroySemaphore(m_vkd->device(), sem.acquire, nullptr);
      m_vkd->vkDestroySemaphore(m_vkd->device(), sem.present, nullptr);
      m_vkd->vkDestroyFence(m_vkd->device(), sem.fence, nullptr);
    }

    // The conditional is here because some third party layers don't properly handle null swapchains
    if (m_swapchain)
      m_vkd->vkDestroySwapchainKHR(m_vkd->device(), m_swapchain, nullptr);

    m_images.clear();
    m_semaphores.clear();
    m_dynamicModes.clear();

    m_swapchain = VK_NULL_HANDLE;
    m_acquireStatus = VK_NOT_READY;

    m_presentPending = false;

    m_hdrMetadataDirty = true;

    m_latencySleepModeDirty = true;
    m_latencySleepSupported = false;

    m_hasPresentId = false;
    m_hasPresentWait = false;

    m_timingDomains = std::nullopt;
    m_timingDisplayInfo = std::nullopt;
    m_timingMode = PresenterTimingInfo();
  }


  void Presenter::destroySurface() {
    m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);

    m_surface = VK_NULL_HANDLE;
  }


  void Presenter::destroyLatencySemaphore() {
    m_vkd->vkDestroySemaphore(m_vkd->device(), m_latencySemaphore, nullptr);

    m_latencySemaphore = VK_NULL_HANDLE;
  }


  void Presenter::waitForSwapchainFence(
          PresenterSync&            sync) {
    if (!sync.fenceSignaled)
      return;

    VkResult vr = m_vkd->vkWaitForFences(m_vkd->device(),
      1, &sync.fence, VK_TRUE, ~0ull);

    if (vr)
      Logger::err(str::format("Presenter: Failed to wait for WSI fence: ", vr));

    if ((vr = m_vkd->vkResetFences(m_vkd->device(), 1, &sync.fence)))
      Logger::err(str::format("Presenter: Failed to reset WSI fence: ", vr));

    sync.fenceSignaled = VK_FALSE;
  }


  void Presenter::pushFrame(const PresenterFrame& frame) {
    std::unique_lock lock(m_frameMutex);

    // This should realistically never stall; this acts more as a safeguard
    // in case the frame worker is being starved by the system.
    m_frameDrain.wait(lock, [this] {
      return m_frameQueuePushId - m_frameQueuePopId < m_frameQueue.size();
    });

    m_frameQueue[m_frameQueuePushId % m_frameQueue.size()] = frame;
    m_frameQueuePushId += 1u;

    m_frameCond.notify_one();
  }


  void Presenter::runFrameThread() {
    env::setThreadName("dxvk-frame");

    while (true) {
      PresenterFrame frame = { };

      // Wait for all GPU work for this frame to complete in order to maintain
      // ordering guarantees of the frame signal w.r.t. objects being released
      { std::unique_lock lock(m_frameMutex);

        m_frameCond.wait(lock, [this] {
          return m_frameQueuePushId > m_frameQueuePopId;
        });

        // Use a frame ID of 0 as an exit condition
        frame = m_frameQueue[m_frameQueuePopId % m_frameQueue.size()];

        if (!frame.frameId) {
          m_frameQueuePopId += 1u;
          return;
        }
      }

      // If the present operation has succeeded, actually wait for it to complete.
      // Don't bother with it on MAILBOX / IMMEDIATE modes since doing so would
      // restrict us to the display refresh rate on some platforms (XWayland).
      if (frame.result >= 0 && (frame.mode == VK_PRESENT_MODE_FIFO_KHR || frame.mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
        VkResult vr;

        if (m_device->features().khrPresentWait2.presentWait2) {
          VkPresentWait2InfoKHR waitInfo = { VK_STRUCTURE_TYPE_PRESENT_WAIT_2_INFO_KHR };
          waitInfo.presentId = frame.frameId;
          waitInfo.timeout = std::numeric_limits<uint64_t>::max();

          vr = m_vkd->vkWaitForPresent2KHR(m_vkd->device(), m_swapchain, &waitInfo);
        } else {
          vr = m_vkd->vkWaitForPresentKHR(m_vkd->device(),
            m_swapchain, frame.frameId, std::numeric_limits<uint64_t>::max());
        }

        if (vr < 0 && vr != VK_ERROR_OUT_OF_DATE_KHR && vr != VK_ERROR_SURFACE_LOST_KHR)
          Logger::err(str::format("Presenter: vkWaitForPresentKHR failed: ", vr));
      }

      updatePresentTiming();

      // Signal latency tracker right away to get more accurate
      // measurements if the frame rate limiter is enabled.
      if (frame.tracker) {
        frame.tracker->notifyGpuPresentEnd(frame.frameId);
        frame.tracker = nullptr;
      }

      // Apply FPS limiter here to align it as closely with scanout as we can,
      // and delay signaling the frame latency event to emulate behaviour of a
      // low refresh rate display as closely as we can.
      m_fpsLimiter.delay();

      // Wake up any thread that may be waiting for the queue to become empty
      bool canSignal = false;

      { std::unique_lock lock(m_frameMutex);
        m_frameQueuePopId += 1u;
        m_frameDrain.notify_one();

        m_lastCompleted = frame.frameId;
        canSignal = m_lastSignaled >= frame.frameId;
      }

      // Always signal even on error, since failures here
      // are transparent to the front-end.
      if (canSignal)
        m_signal->signal(frame.frameId);
    }
  }


  VkResult Presenter::softError(
          VkResult                  vr) {
    // Don't return these as an error state to the caller. The app can't
    // do much anyway, so just pretend that we don't have a valid swap
    // chain and move on. An alternative would be to handle errors in a
    // loop, however this may also not be desireable since it could stall
    // the app indefinitely in case the surface is in a weird state.
    if (vr == VK_ERROR_SURFACE_LOST_KHR || vr == VK_ERROR_OUT_OF_DATE_KHR)
      return VK_NOT_READY;

    return vr;
  }

}
