#pragma once

#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Semaphore object
   * 
   * This is merely an abstraction of Vulkan's semaphores.
   * They are only used internally by \ref DxvkSwapchain
   * in order to synchronize the presentation engine with
   * command buffer submissions.
   */
  class DxvkSemaphore : public DxvkResource {
    
  public:
    
    DxvkSemaphore(const Rc<vk::DeviceFn>& vkd);
    ~DxvkSemaphore();
    
    /**
     * \brief Semaphore handle
     * 
     * Internal use only.
     * \returns Semaphore handle
     */
    VkSemaphore handle() const {
      return m_semaphore;
    }
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    VkSemaphore       m_semaphore;
    
  };
  
  
  /**
   * \brief Fence object
   * 
   * This is merely an abstraction of Vulkan's fences. Client
   * APIs that support fence operations may use them directly.
   * Other than that, they are used internally to keep track
   * of GPU resource usage.
   */
  class DxvkFence : public RcObject {
    
  public:
    
    DxvkFence(const Rc<vk::DeviceFn>& vkd);
    ~DxvkFence();
    
    /**
     * \brief Fence handle
     * 
     * Internal use only.
     * \returns Fence handle
     */
    VkFence handle() const {
      return m_fence;
    }
    
    /**
     * \brief Waits for fence to be signaled
     * 
     * \param [in] timeout Amount of time to wait
     * \returns \c true if the fence has been signaled,
     *          \c false if a timeout occured.
     */
    bool wait(uint64_t timeout) const;
    
    /**
     * \brief Resets the fence
     * 
     * Transitions the fence into the unsignaled state,
     * which means that the fence may be submitted again.
     */
    void reset();
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    VkFence           m_fence;
    
  };
  
}