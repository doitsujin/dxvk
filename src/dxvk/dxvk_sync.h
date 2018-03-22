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
  
}