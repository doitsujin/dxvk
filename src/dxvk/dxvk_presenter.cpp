#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_presenter.h"

#include "../wsi/wsi_window.h"

namespace dxvk {

  Presenter::Presenter(
    const Rc<DxvkDevice>&   device,
    const Rc<sync::Signal>& signal,
    const PresenterDesc&    desc)
  : m_device(device), m_signal(signal),
    m_vki(device->instance()->vki()),
    m_vkd(device->vkd()) {
    // If a frame signal was provided, launch thread that synchronizes
    // with present operations and periodically signals the event
    if (m_device->features().khrPresentWait.presentWait && m_signal != nullptr)
      m_frameThread = dxvk::thread([this] { runFrameThread(); });
  }

  
  Presenter::~Presenter() {
    destroySwapchain();
    destroySurface();

    if (m_frameThread.joinable()) {
      { std::lock_guard<dxvk::mutex> lock(m_frameMutex);

        m_frameQueue.push(PresenterFrame());
        m_frameCond.notify_one();
      }

      m_frameThread.join();
    }
  }


  PresenterInfo Presenter::info() const {
    return m_info;
  }


  PresenterImage Presenter::getImage(uint32_t index) const {
    return m_images.at(index);
  }


  VkResult Presenter::acquireNextImage(PresenterSync& sync, uint32_t& index) {
    sync = m_semaphores.at(m_frameIndex);

    // Don't acquire more than one image at a time
    if (m_acquireStatus == VK_NOT_READY) {
      m_acquireStatus = m_vkd->vkAcquireNextImageKHR(m_vkd->device(),
        m_swapchain, std::numeric_limits<uint64_t>::max(),
        sync.acquire, VK_NULL_HANDLE, &m_imageIndex);
    }

    if (m_acquireStatus != VK_SUCCESS && m_acquireStatus != VK_SUBOPTIMAL_KHR)
      return m_acquireStatus;
    
    index = m_imageIndex;
    return m_acquireStatus;
  }


  VkResult Presenter::presentImage(
          VkPresentModeKHR  mode,
          uint64_t          frameId) {
    PresenterSync sync = m_semaphores.at(m_frameIndex);

    VkPresentIdKHR presentId = { VK_STRUCTURE_TYPE_PRESENT_ID_KHR };
    presentId.swapchainCount = 1;
    presentId.pPresentIds   = &frameId;

    VkSwapchainPresentModeInfoEXT modeInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT };
    modeInfo.swapchainCount = 1;
    modeInfo.pPresentModes  = &mode;

    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &sync.present;
    info.swapchainCount     = 1;
    info.pSwapchains        = &m_swapchain;
    info.pImageIndices      = &m_imageIndex;

    if (m_device->features().khrPresentId.presentId && frameId)
      presentId.pNext = const_cast<void*>(std::exchange(info.pNext, &presentId));

    if (m_device->features().extSwapchainMaintenance1.swapchainMaintenance1)
      modeInfo.pNext = const_cast<void*>(std::exchange(info.pNext, &modeInfo));

    VkResult status = m_vkd->vkQueuePresentKHR(
      m_device->queues().graphics.queueHandle, &info);

    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)
      return status;

    // Try to acquire next image already, in order to hide
    // potential delays from the application thread.
    m_frameIndex += 1;
    m_frameIndex %= m_semaphores.size();

    sync = m_semaphores.at(m_frameIndex);

    m_acquireStatus = m_vkd->vkAcquireNextImageKHR(m_vkd->device(),
      m_swapchain, std::numeric_limits<uint64_t>::max(),
      sync.acquire, VK_NULL_HANDLE, &m_imageIndex);

    return status;
  }


  void Presenter::signalFrame(
          VkResult          result,
          VkPresentModeKHR  mode,
          uint64_t          frameId) {
    if (m_signal == nullptr || !frameId)
      return;

    if (m_device->features().khrPresentWait.presentWait) {
      std::lock_guard<dxvk::mutex> lock(m_frameMutex);

      PresenterFrame frame = { };
      frame.result = result;
      frame.mode = mode;
      frame.frameId = frameId;

      m_frameQueue.push(frame);
      m_frameCond.notify_one();
    } else {
      applyFrameRateLimit(mode);
      m_signal->signal(frameId);
    }

    m_lastFrameId.store(frameId, std::memory_order_release);
  }


  VkResult Presenter::recreateSurface(
    const std::function<VkResult (VkSurfaceKHR*)>& fn) {
    if (m_swapchain)
      destroySwapchain();

    if (m_surface)
      destroySurface();

    return fn(&m_surface);
  }


  VkResult Presenter::recreateSwapChain(const PresenterDesc& desc) {
    if (m_swapchain)
      destroySwapchain();

    if (!m_surface)
      return VK_ERROR_SURFACE_LOST_KHR;

    VkSurfaceFullScreenExclusiveInfoEXT fullScreenExclusiveInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenExclusiveInfo.fullScreenExclusive = desc.fullScreenExclusive;

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

    if (status)
      return status;

    // Select image extent based on current surface capabilities, and return
    // immediately if we cannot create an actual swap chain.
    m_info.imageExtent = pickImageExtent(caps.surfaceCapabilities, desc.imageExtent);

    if (!m_info.imageExtent.width || !m_info.imageExtent.height) {
      m_info.imageCount = 0;
      m_info.format     = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      return VK_SUCCESS;
    }

    // Select format based on swap chain properties
    if ((status = getSupportedFormats(formats, desc.fullScreenExclusive)))
      return status;

    m_info.format = pickFormat(formats.size(), formats.data(), desc.numFormats, desc.formats);

    // Select a present mode for the current sync interval
    if ((status = getSupportedPresentModes(modes, desc.fullScreenExclusive)))
      return status;

    m_info.presentMode = pickPresentMode(modes.size(), modes.data(), m_info.syncInterval);

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
      presentModeInfo.presentMode = m_info.presentMode;

      caps.pNext = &compatibleModeInfo;

      if ((status = m_vki->vkGetPhysicalDeviceSurfaceCapabilities2KHR(
          m_device->adapter()->handle(), &surfaceInfo, &caps)))
        return status;

      compatibleModes.resize(compatibleModeInfo.presentModeCount);
      compatibleModeInfo.pPresentModes = compatibleModes.data();

      if ((status = m_vki->vkGetPhysicalDeviceSurfaceCapabilities2KHR(
          m_device->adapter()->handle(), &surfaceInfo, &caps)))
        return status;

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
            m_device->adapter()->handle(), &surfaceInfo, &caps)))
          return status;

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
    m_info.imageCount = pickImageCount(minImageCount, maxImageCount, desc.imageCount);

    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = desc.fullScreenExclusive;

    VkSwapchainPresentModesCreateInfoEXT modeInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT };
    modeInfo.presentModeCount       = compatibleModes.size();
    modeInfo.pPresentModes          = compatibleModes.data();

    VkSwapchainCreateInfoKHR swapInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapInfo.surface                = m_surface;
    swapInfo.minImageCount          = m_info.imageCount;
    swapInfo.imageFormat            = m_info.format.format;
    swapInfo.imageColorSpace        = m_info.format.colorSpace;
    swapInfo.imageExtent            = m_info.imageExtent;
    swapInfo.imageArrayLayers       = 1;
    swapInfo.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapInfo.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform           = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapInfo.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode            = m_info.presentMode;
    swapInfo.clipped                = VK_TRUE;

    if (m_device->features().extFullScreenExclusive)
      fullScreenInfo.pNext = const_cast<void*>(std::exchange(swapInfo.pNext, &fullScreenInfo));

    if (m_device->features().extSwapchainMaintenance1.swapchainMaintenance1)
      modeInfo.pNext = std::exchange(swapInfo.pNext, &modeInfo);

    Logger::info(str::format(
      "Presenter: Actual swap chain properties:"
      "\n  Format:       ", m_info.format.format,
      "\n  Color space:  ", m_info.format.colorSpace,
      "\n  Present mode: ", m_info.presentMode, " (dynamic: ", (dynamicModes.empty() ? "no)" : "yes)"),
      "\n  Buffer size:  ", m_info.imageExtent.width, "x", m_info.imageExtent.height,
      "\n  Image count:  ", m_info.imageCount,
      "\n  Exclusive FS: ", desc.fullScreenExclusive));
    
    if ((status = m_vkd->vkCreateSwapchainKHR(m_vkd->device(),
        &swapInfo, nullptr, &m_swapchain)))
      return status;
    
    // Acquire images and create views
    std::vector<VkImage> images;

    if ((status = getSwapImages(images)))
      return status;
    
    // Update actual image count
    m_info.imageCount = images.size();
    m_images.resize(m_info.imageCount);

    for (uint32_t i = 0; i < m_info.imageCount; i++) {
      m_images[i].image = images[i];

      VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
      viewInfo.image    = images[i];
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format   = m_info.format.format;
      viewInfo.components = VkComponentMapping {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
      viewInfo.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1, 0, 1 };
      
      if ((status = m_vkd->vkCreateImageView(m_vkd->device(),
          &viewInfo, nullptr, &m_images[i].view)))
        return status;
    }

    // Create one set of semaphores per swap image
    m_semaphores.resize(m_info.imageCount);

    for (uint32_t i = 0; i < m_semaphores.size(); i++) {
      VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

      if ((status = m_vkd->vkCreateSemaphore(m_vkd->device(),
          &semInfo, nullptr, &m_semaphores[i].acquire)))
        return status;

      if ((status = m_vkd->vkCreateSemaphore(m_vkd->device(),
          &semInfo, nullptr, &m_semaphores[i].present)))
        return status;
    }
    
    // Invalidate indices
    m_imageIndex = 0;
    m_frameIndex = 0;
    m_acquireStatus = VK_NOT_READY;

    m_dynamicModes = std::move(dynamicModes);
    return VK_SUCCESS;
  }


  bool Presenter::supportsColorSpace(VkColorSpaceKHR colorspace) {
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    getSupportedFormats(surfaceFormats, VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT);

    for (const auto& surfaceFormat : surfaceFormats) {
      if (surfaceFormat.colorSpace == colorspace)
        return true;
    }

    return false;
  }


  VkResult Presenter::setSyncInterval(uint32_t syncInterval) {
    // Normalize sync interval for present modes. We currently
    // cannot support anything other than 1 natively anyway.
    syncInterval = std::min(syncInterval, 1u);

    if (syncInterval == m_info.syncInterval)
      return VK_SUCCESS;

    m_info.syncInterval = syncInterval;

    if (syncInterval >= m_dynamicModes.size())
      return VK_ERROR_OUT_OF_DATE_KHR;

    m_info.presentMode = m_dynamicModes[syncInterval];
    return VK_SUCCESS;
  }


  void Presenter::setFrameRateLimit(double frameRate) {
    m_fpsLimiter.setTargetFrameRate(frameRate);
  }


  void Presenter::setHdrMetadata(const VkHdrMetadataEXT& hdrMetadata) {
    if (m_device->features().extHdrMetadata)
      m_vkd->vkSetHdrMetadataEXT(m_vkd->device(), 1, &m_swapchain, &hdrMetadata);
  }


  VkResult Presenter::getSupportedFormats(std::vector<VkSurfaceFormatKHR>& formats, VkFullScreenExclusiveEXT fullScreenExclusive) const {
    uint32_t numFormats = 0;

    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = fullScreenExclusive;

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

    if (status != VK_SUCCESS)
      return status;
    
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

    return status;
  }

  
  VkResult Presenter::getSupportedPresentModes(std::vector<VkPresentModeKHR>& modes, VkFullScreenExclusiveEXT fullScreenExclusive) const {
    uint32_t numModes = 0;

    VkSurfaceFullScreenExclusiveInfoEXT fullScreenInfo = { VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT };
    fullScreenInfo.fullScreenExclusive = fullScreenExclusive;

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

    if (status != VK_SUCCESS)
      return status;
    
    modes.resize(numModes);

    if (m_device->features().extFullScreenExclusive) {
      status = m_vki->vkGetPhysicalDeviceSurfacePresentModes2EXT(
        m_device->adapter()->handle(), &surfaceInfo, &numModes, modes.data());
    } else {
      status = m_vki->vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_device->adapter()->handle(), m_surface, &numModes, modes.data());
    }

    return status;
  }


  VkResult Presenter::getSwapImages(std::vector<VkImage>& images) {
    uint32_t imageCount = 0;

    VkResult status = m_vkd->vkGetSwapchainImagesKHR(
      m_vkd->device(), m_swapchain, &imageCount, nullptr);
    
    if (status != VK_SUCCESS)
      return status;
    
    images.resize(imageCount);

    return m_vkd->vkGetSwapchainImagesKHR(
      m_vkd->device(), m_swapchain, &imageCount, images.data());
  }


  VkSurfaceFormatKHR Presenter::pickFormat(
          uint32_t                  numSupported,
    const VkSurfaceFormatKHR*       pSupported,
          uint32_t                  numDesired,
    const VkSurfaceFormatKHR*       pDesired) {
    if (numDesired > 0) {
      // If the implementation allows us to freely choose
      // the format, we'll just use the preferred format.
      if (numSupported == 1 && pSupported[0].format == VK_FORMAT_UNDEFINED)
        return pDesired[0];
      
      // If the preferred format is explicitly listed in
      // the array of supported surface formats, use it
      for (uint32_t i = 0; i < numDesired; i++) {
        for (uint32_t j = 0; j < numSupported; j++) {
          if (pSupported[j].format     == pDesired[i].format
           && pSupported[j].colorSpace == pDesired[i].colorSpace)
            return pSupported[j];
        }
      }

      // If that didn't work, we'll fall back to a format
      // which has similar properties to the preferred one
      DxvkFormatFlags prefFlags = lookupFormatInfo(pDesired[0].format)->flags;

      for (uint32_t j = 0; j < numSupported; j++) {
        auto currFlags = lookupFormatInfo(pSupported[j].format)->flags;

        if ((currFlags & DxvkFormatFlag::ColorSpaceSrgb)
         == (prefFlags & DxvkFormatFlag::ColorSpaceSrgb))
          return pSupported[j];
      }
    }
    
    // Otherwise, fall back to the first supported format
    return pSupported[0];
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
          uint32_t                  maxImageCount,
          uint32_t                  desired) {
    uint32_t count = minImageCount + 1;
    
    if (count < desired)
      count = desired;
    
    if (count > maxImageCount && maxImageCount != 0)
      count = maxImageCount;
    
    return count;
  }


  void Presenter::destroySwapchain() {
    if (m_signal != nullptr)
      m_signal->wait(m_lastFrameId.load(std::memory_order_acquire));

    for (const auto& img : m_images)
      m_vkd->vkDestroyImageView(m_vkd->device(), img.view, nullptr);
    
    for (const auto& sem : m_semaphores) {
      m_vkd->vkDestroySemaphore(m_vkd->device(), sem.acquire, nullptr);
      m_vkd->vkDestroySemaphore(m_vkd->device(), sem.present, nullptr);
    }

    m_vkd->vkDestroySwapchainKHR(m_vkd->device(), m_swapchain, nullptr);

    m_images.clear();
    m_semaphores.clear();
    m_dynamicModes.clear();

    m_swapchain = VK_NULL_HANDLE;
  }


  void Presenter::destroySurface() {
    m_vki->vkDestroySurfaceKHR(m_vki->instance(), m_surface, nullptr);

    m_surface = VK_NULL_HANDLE;
  }


  void Presenter::applyFrameRateLimit(VkPresentModeKHR mode) {
    bool vsync = mode == VK_PRESENT_MODE_FIFO_KHR
              || mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR;

    m_fpsLimiter.delay(vsync);
  }


  void Presenter::runFrameThread() {
    env::setThreadName("dxvk-frame");

    while (true) {
      std::unique_lock<dxvk::mutex> lock(m_frameMutex);

      m_frameCond.wait(lock, [this] {
        return !m_frameQueue.empty();
      });

      PresenterFrame frame = m_frameQueue.front();
      m_frameQueue.pop();

      lock.unlock();

      // Use a frame ID of 0 as an exit condition
      if (!frame.frameId)
        return;

      // Apply the FPS limiter before signaling the frame event in
      // order to reduce latency if the app uses it for frame pacing.
      applyFrameRateLimit(frame.mode);

      // If the present operation has succeeded, actually wait for it to complete.
      // Don't bother with it on MAILBOX / IMMEDIATE modes since doing so would
      // restrict us to the display refresh rate on some platforms (XWayland).
      if (frame.result >= 0 && (frame.mode == VK_PRESENT_MODE_FIFO_KHR || frame.mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
        VkResult vr = m_vkd->vkWaitForPresentKHR(m_vkd->device(),
          m_swapchain, frame.frameId, std::numeric_limits<uint64_t>::max());

        if (vr < 0 && vr != VK_ERROR_OUT_OF_DATE_KHR && vr != VK_ERROR_SURFACE_LOST_KHR)
          Logger::err(str::format("Presenter: vkWaitForPresentKHR failed: ", vr));
      }

      // Always signal even on error, since failures here
      // are transparent to the front-end.
      m_signal->signal(frame.frameId);
    }
  }

}
