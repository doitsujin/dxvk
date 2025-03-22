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

    // If a frame signal was provided, launch thread that synchronizes
    // with present operations and periodically signals the event
    if (m_device->features().khrPresentWait.presentWait && m_signal != nullptr)
      m_frameThread = dxvk::thread([this] { runFrameThread(); });
  }

  
  Presenter::~Presenter() {
    destroySwapchain();
    destroySurface();
    destroyLatencySemaphore();

    if (m_frameThread.joinable()) {
      { std::lock_guard lock(m_frameMutex);

        m_frameQueue.push(PresenterFrame());
        m_frameCond.notify_one();
      }

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

    VkPresentIdKHR presentId = { VK_STRUCTURE_TYPE_PRESENT_ID_KHR };
    presentId.swapchainCount = 1;
    presentId.pPresentIds   = &frameId;

    VkSwapchainPresentFenceInfoEXT fenceInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT };
    fenceInfo.swapchainCount = 1;
    fenceInfo.pFences       = &currSync.fence;

    VkSwapchainPresentModeInfoEXT modeInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT };
    modeInfo.swapchainCount = 1;
    modeInfo.pPresentModes  = &m_presentMode;

    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &currSync.present;
    info.swapchainCount     = 1;
    info.pSwapchains        = &m_swapchain;
    info.pImageIndices      = &m_imageIndex;

    if (m_device->features().khrPresentId.presentId && frameId)
      presentId.pNext = const_cast<void*>(std::exchange(info.pNext, &presentId));

    if (m_device->features().extSwapchainMaintenance1.swapchainMaintenance1) {
      modeInfo.pNext = const_cast<void*>(std::exchange(info.pNext, &modeInfo));
      fenceInfo.pNext = const_cast<void*>(std::exchange(info.pNext, &fenceInfo));
    }

    VkResult status = m_vkd->vkQueuePresentKHR(
      m_device->queues().graphics.queueHandle, &info);

    // Maintain valid state if presentation succeeded, even if we want to
    // recreate the swapchain. Spec says that 'queue' operations, i.e. the
    // semaphore and fence signals, still happen if present fails with
    // normal swapchain errors, such as OUT_OF_DATE or SURFACE_LOST.
    if (m_device->features().extSwapchainMaintenance1.swapchainMaintenance1) {
      currSync.fenceSignaled = status != VK_ERROR_OUT_OF_DEVICE_MEMORY
                            && status != VK_ERROR_OUT_OF_HOST_MEMORY
                            && status != VK_ERROR_DEVICE_LOST;
    }

    if (status >= 0) {
      m_acquireStatus = VK_NOT_READY;

      m_frameIndex += 1;
      m_frameIndex %= m_semaphores.size();
    }

    // Add frame to waiter queue with current properties
    if (m_device->features().khrPresentWait.presentWait) {
      std::lock_guard lock(m_frameMutex);

      auto& frame = m_frameQueue.emplace();
      frame.frameId = frameId;
      frame.tracker = tracker;
      frame.mode = m_presentMode;
      frame.result = status;

      m_frameCond.notify_one();
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

    if (m_device->features().khrPresentWait.presentWait) {
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
    VkSurfaceCapabilities2KHR caps = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };

    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;

    VkResult status;

    if (m_device->features().extFullScreenExclusive) {
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

    if (m_device->features().extSwapchainMaintenance1.swapchainMaintenance1) {
      VkSurfacePresentModeCompatibilityEXT compatibleModeInfo = { VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT };

      VkSurfacePresentModeEXT presentModeInfo = { VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT };
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

    // Compute swap chain image count based on available info
    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = m_fullscreenMode;

    VkSwapchainPresentModesCreateInfoEXT modeInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT };
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

    if (m_device->features().extFullScreenExclusive)
      fullScreenInfo.pNext = const_cast<void*>(std::exchange(swapInfo.pNext, &fullScreenInfo));

    if (m_device->features().extSwapchainMaintenance1.swapchainMaintenance1)
      modeInfo.pNext = std::exchange(swapInfo.pNext, &modeInfo);

    if (m_device->features().nvLowLatency2)
      latencyInfo.pNext = std::exchange(swapInfo.pNext, &latencyInfo);

    Logger::info(str::format(
      "Presenter: Actual swapchain properties:"
      "\n  Format:       ", swapInfo.imageFormat,
      "\n  Color space:  ", swapInfo.imageColorSpace,
      "\n  Present mode: ", swapInfo.presentMode, " (dynamic: ", (dynamicModes.empty() ? "no)" : "yes)"),
      "\n  Buffer size:  ", swapInfo.imageExtent.width, "x", swapInfo.imageExtent.height,
      "\n  Image count:  ", swapInfo.minImageCount));
    
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

    if (!m_device->features().extSwapchainMaintenance1.swapchainMaintenance1) {
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
    if (!m_device->features().extSwapchainMaintenance1.swapchainMaintenance1)
      m_device->waitForIdle();

    // Wait for the presentWait worker to finish using
    // the swapchain before destroying it.
    std::unique_lock lock(m_frameMutex);

    m_frameDrain.wait(lock, [this] {
      return m_frameQueue.empty();
    });

    for (auto& sem : m_semaphores)
      waitForSwapchainFence(sem);

    for (const auto& sem : m_semaphores) {
      m_vkd->vkDestroySemaphore(m_vkd->device(), sem.acquire, nullptr);
      m_vkd->vkDestroySemaphore(m_vkd->device(), sem.present, nullptr);
      m_vkd->vkDestroyFence(m_vkd->device(), sem.fence, nullptr);
    }

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


  void Presenter::runFrameThread() {
    env::setThreadName("dxvk-frame");

    while (true) {
      PresenterFrame frame = { };

      // Wait for all GPU work for this frame to complete in order to maintain
      // ordering guarantees of the frame signal w.r.t. objects being released
      { std::unique_lock lock(m_frameMutex);

        m_frameCond.wait(lock, [this] {
          return !m_frameQueue.empty();
        });

        // Use a frame ID of 0 as an exit condition
        frame = m_frameQueue.front();

        if (!frame.frameId) {
          m_frameQueue.pop();
          return;
        }
      }

      // If the present operation has succeeded, actually wait for it to complete.
      // Don't bother with it on MAILBOX / IMMEDIATE modes since doing so would
      // restrict us to the display refresh rate on some platforms (XWayland).
      if (frame.result >= 0 && (frame.mode == VK_PRESENT_MODE_FIFO_KHR || frame.mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
        VkResult vr = m_vkd->vkWaitForPresentKHR(m_vkd->device(),
          m_swapchain, frame.frameId, std::numeric_limits<uint64_t>::max());

        if (vr < 0 && vr != VK_ERROR_OUT_OF_DATE_KHR && vr != VK_ERROR_SURFACE_LOST_KHR)
          Logger::err(str::format("Presenter: vkWaitForPresentKHR failed: ", vr));
      }

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

        m_frameQueue.pop();
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
