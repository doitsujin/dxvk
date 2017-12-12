#pragma once

#include "dxvk_framebuffer.h"
#include "dxvk_sync.h"

namespace dxvk {
 
  class DxvkDevice;
  class DxvkFramebuffer;
  class DxvkSurface;
  
  /**
   * \brief 
   */
  struct DxvkSwapchainProperties {
    VkSurfaceFormatKHR preferredSurfaceFormat;
    VkPresentModeKHR   preferredPresentMode;
    VkExtent2D         preferredBufferSize;
  };
  
  
  /**
   * \brief DXVK swapchain
   * 
   * Manages a Vulkan swapchain object.
   */
  class DxvkSwapchain : public RcObject {
    
  public:
    
    DxvkSwapchain(
      const Rc<DxvkDevice>&           device,
      const Rc<DxvkSurface>&          surface,
      const DxvkSwapchainProperties&  properties);
    ~DxvkSwapchain();
    
    /**
     * \brief Retrieves the framebuffer for the current frame
     * 
     * If necessary, this will automatically recreate the
     * underlying swapchain object and framebuffer objects.
     * \param [in] wakeSync Semaphore to signal
     * \returns The framebuffer object
     */
    Rc<DxvkFramebuffer> getFramebuffer(
      const Rc<DxvkSemaphore>& wakeSync);
    
    /**
     * \brief Presents the current framebuffer
     * 
     * This may actually fail to present an image. If that is the
     * case, the surface contents will be undefined for this frame
     * and the swapchain object will be recreated.
     * \param [in] waitSync Semaphore to wait on
     */
    void present(
      const Rc<DxvkSemaphore>& waitSync);
    
    /**
     * \brief Changes swapchain properties
     * 
     * This must not be called between \ref getFramebuffer
     * and \ref present as this method may recreate the swap
     * chain and framebuffer objects immediately.
     * \param [in] props New swapchain properties
     */
    void changeProperties(
      const DxvkSwapchainProperties& props);
    
  private:
    
    Rc<DxvkDevice>          m_device;
    Rc<vk::DeviceFn>        m_vkd;
    Rc<DxvkSurface>         m_surface;
    
    DxvkSwapchainProperties m_properties;
    VkSwapchainKHR          m_handle     = VK_NULL_HANDLE;
    uint32_t                m_imageIndex = 0;
    uint32_t                m_frameIndex = 0;
    
    Rc<DxvkRenderPass>               m_renderPass;
    std::vector<Rc<DxvkFramebuffer>> m_framebuffers;
    
    VkResult acquireNextImage(
      const Rc<DxvkSemaphore>& wakeSync);
    
    void recreateSwapchain();
    
    std::vector<VkImage> retrieveSwapImages();
    
  };
  
}