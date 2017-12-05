#include "dxvk_main.h"

#include "dxvk_device.h"
#include "dxvk_framebuffer.h"
#include "dxvk_surface.h"
#include "dxvk_swapchain.h"

namespace dxvk {
  
  DxvkSwapchain::DxvkSwapchain(
    const Rc<DxvkDevice>&           device,
    const Rc<DxvkSurface>&          surface,
    const DxvkSwapchainProperties&  properties,
          VkQueue                   queue)
  : m_device    (device),
    m_vkd       (device->vkd()),
    m_surface   (surface),
    m_queue     (queue),
    m_properties(properties) {
    this->recreateSwapchain();
  }
  
  
  DxvkSwapchain::~DxvkSwapchain() {
    m_device->waitForIdle();
    m_vkd->vkDestroySwapchainKHR(
      m_vkd->device(), m_handle, nullptr);
  }
  
  
  Rc<DxvkFramebuffer> DxvkSwapchain::getFramebuffer(
    const Rc<DxvkSemaphore>& wakeSync) {
    VkResult status = this->acquireNextImage(wakeSync);
    
    if (status == VK_ERROR_OUT_OF_DATE_KHR) {
      this->recreateSwapchain();
      status = this->acquireNextImage(wakeSync);
    }
    
    if (status != VK_SUCCESS
     && status != VK_SUBOPTIMAL_KHR)
      throw DxvkError("DxvkSwapchain::getFramebuffer: Failed to acquire image");
    
    return m_framebuffers.at(m_imageIndex);
  }
  
  
  void DxvkSwapchain::present(const Rc<DxvkSemaphore>& waitSync) {
    const VkSemaphore waitSemaphore = waitSync->handle();
    
    VkPresentInfoKHR info;
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext              = nullptr;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &waitSemaphore;
    info.swapchainCount     = 1;
    info.pSwapchains        = &m_handle;
    info.pImageIndices      = &m_imageIndex;
    info.pResults           = nullptr;
    
    VkResult status = m_vkd->vkQueuePresentKHR(m_queue, &info);
    
    if (status != VK_SUCCESS
     && status != VK_SUBOPTIMAL_KHR
     && status != VK_ERROR_OUT_OF_DATE_KHR)
      throw DxvkError("DxvkSwapchain::present: Failed to present image");
  }
  
  
  void DxvkSwapchain::changeProperties(
    const DxvkSwapchainProperties& props) {
    m_properties = props;
    this->recreateSwapchain();
  }
  
  
  VkResult DxvkSwapchain::acquireNextImage(
    const Rc<DxvkSemaphore>& wakeSync) {
    return m_vkd->vkAcquireNextImageKHR(
      m_vkd->device(), m_handle,
      std::numeric_limits<uint64_t>::max(),
      wakeSync->handle(),
      VK_NULL_HANDLE,
      &m_imageIndex);
  }
  
  
  void DxvkSwapchain::recreateSwapchain() {
    VkSwapchainKHR oldSwapchain = m_handle;
    
    // Wait until we can be certain that none of our
    // resources are still in use by the device.
    m_device->waitForIdle();
    
    // Recreate the actual swapchain object
    auto caps = m_surface->getSurfaceCapabilities();
    auto fmt  = m_surface->pickSurfaceFormat(1, &m_properties.preferredSurfaceFormat);
    auto mode = m_surface->pickPresentMode  (1, &m_properties.preferredPresentMode);
    
    VkSwapchainCreateInfoKHR swapInfo;
    swapInfo.sType                  = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.pNext                  = nullptr;
    swapInfo.flags                  = 0;
    swapInfo.surface                = m_surface->handle();
    swapInfo.minImageCount          = m_surface->pickImageCount(caps, mode);
    swapInfo.imageFormat            = fmt.format;
    swapInfo.imageColorSpace        = fmt.colorSpace;
    swapInfo.imageExtent            = m_surface->pickImageExtent(caps, m_properties.preferredBufferSize);
    swapInfo.imageArrayLayers       = 1;
    swapInfo.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapInfo.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.queueFamilyIndexCount  = 0;
    swapInfo.pQueueFamilyIndices    = nullptr;
    swapInfo.preTransform           = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapInfo.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapInfo.presentMode            = mode;
    swapInfo.clipped                = VK_TRUE;
    swapInfo.oldSwapchain           = oldSwapchain;
    
    if (m_vkd->vkCreateSwapchainKHR(m_vkd->device(), &swapInfo, nullptr, &m_handle) != VK_SUCCESS)
      throw DxvkError("DxvkSwapchain::recreateSwapchain: Failed to recreate swap chain");
    
    // Destroy previous swapchain object
    m_vkd->vkDestroySwapchainKHR(
      m_vkd->device(), oldSwapchain, nullptr);
    
    // Create the render pass object
    DxvkRenderPassFormat renderTargetFormat;
    renderTargetFormat.setColorFormat(0, fmt.format);
    
    m_renderPass = new DxvkRenderPass(
      m_vkd, renderTargetFormat);
    
    // Retrieve swap images
    auto swapImages = this->retrieveSwapImages();
    m_framebuffers.resize(swapImages.size());
    
    DxvkImageCreateInfo imageInfo;
    imageInfo.type          = VK_IMAGE_TYPE_2D;
    imageInfo.format        = fmt.format;
    imageInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent.width  = swapInfo.imageExtent.width;
    imageInfo.extent.height = swapInfo.imageExtent.height;
    imageInfo.extent.depth  = 1;
    imageInfo.numLayers     = swapInfo.imageArrayLayers;
    imageInfo.mipLevels     = 1;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.stages        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    imageInfo.access        = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_MEMORY_READ_BIT;
    imageInfo.layout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format       = fmt.format;
    viewInfo.aspect       = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel     = 0;
    viewInfo.numLevels    = 1;
    viewInfo.minLayer     = 0;
    viewInfo.numLayers    = swapInfo.imageArrayLayers;
    
    for (size_t i = 0; i < swapImages.size(); i++) {
      Rc<DxvkImage> image = new DxvkImage(m_vkd, imageInfo, swapImages.at(i));
      Rc<DxvkImageView> iview = m_device->createImageView(image, viewInfo);
      
      DxvkRenderTargets renderTargets;
      renderTargets.setColorTarget(0, iview);
      
      m_framebuffers.at(i) = new DxvkFramebuffer(
        m_vkd, m_renderPass, renderTargets);
    }
  }
  
  
  std::vector<VkImage> DxvkSwapchain::retrieveSwapImages() {
    uint32_t imageCount = 0;
    if (m_vkd->vkGetSwapchainImagesKHR(m_vkd->device(), m_handle, &imageCount, nullptr) != VK_SUCCESS)
      throw DxvkError("DxvkSwapchain::recreateSwapchain: Failed to retrieve swap chain images");
    
    std::vector<VkImage> images(imageCount);
    if (m_vkd->vkGetSwapchainImagesKHR(m_vkd->device(), m_handle, &imageCount, images.data()) != VK_SUCCESS)
      throw DxvkError("DxvkSwapchain::recreateSwapchain: Failed to retrieve swap chain images");
    return images;
  }
  
}