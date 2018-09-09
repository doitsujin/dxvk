#pragma once

#include "dxvk_framebuffer.h"
#include "dxvk_sync.h"

namespace dxvk {
 
  class DxvkDevice;
  class DxvkFramebuffer;
  class DxvkSurface;
  
  /**
   * \brief Swap chain semaphore pair
   * 
   * Holds the two semaphores requires for
   * synchronizing swap chain operations.
   */
  struct DxvkSwapSemaphores {
    Rc<DxvkSemaphore> acquireSync; ///< Post-acquire semaphore
    Rc<DxvkSemaphore> presentSync; ///< Pre-present semaphore
  };
  
  
  /**
   * \brief Swap chain properties
   */
  struct DxvkSwapchainProperties {
    VkSurfaceFormatKHR preferredSurfaceFormat;
    VkPresentModeKHR   preferredPresentMode;
    VkExtent2D         preferredBufferSize;
    uint32_t           preferredBufferCount;
  };
  
  
  /**
   * \brief DXVK swapchain
   * 
   * Manages a Vulkan swap chain object. Implements
   * acquire and present methods and recreates the
   * underlying swap chain object as necessary.
   */
  class DxvkSwapchain : public RcObject {
    
  public:
    
    DxvkSwapchain(
      const Rc<DxvkDevice>&           device,
      const Rc<DxvkSurface>&          surface,
      const DxvkSwapchainProperties&  properties);
    ~DxvkSwapchain();
    
    /**
     * \brief Acquires a pair of semaphores
     * 
     * Retrieves a set of semaphores for the acquire
     * and present operations. This must be called
     * \e before \c getImageView.
     * \returns Semaphore pair
     */
    DxvkSwapSemaphores getSemaphorePair();
    
    /**
     * \brief Retrieves the image view for the current frame
     * 
     * If necessary, this will automatically recreate the
     * underlying swapchain object and image view objects.
     * \param [in] wakeSync Semaphore to signal
     * \returns The image view object
     */
    Rc<DxvkImageView> getImageView(
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
     * This must not be called between \ref getImageView
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
    
    std::vector<Rc<DxvkImageView>>    m_framebuffers;
    std::vector<DxvkSwapSemaphores>   m_semaphoreSet;
    
    VkResult acquireNextImage(
      const Rc<DxvkSemaphore>& wakeSync);
    
    void recreateSwapchain();
    
    std::vector<VkImage> retrieveSwapImages();
    
  };
  
}