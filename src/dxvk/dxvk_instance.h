#pragma once

#include "dxvk_adapter.h"
#include "dxvk_device.h"

namespace dxvk {
  
  /**
   * \brief DXVK instance
   * 
   * Manages a Vulkan instance and stores a list
   * of adapters. This also provides methods for
   * device creation.
   */
  class DxvkInstance : public RcObject {
    
  public:
    
    DxvkInstance();
    ~DxvkInstance();
    
    /**
     * \brief Vulkan instance functions
     * \returns Vulkan instance functions
     */
    Rc<vk::InstanceFn> vki() const {
      return m_vki;
    }
    
    /**
     * \brief Vulkan instance handle
     * \returns The instance handle
     */
    VkInstance handle() {
      return m_vki->instance();
    }
    
    /**
     * \brief Retrieves a list of adapters
     * \returns List of adapter objects
     */
    std::vector<Rc<DxvkAdapter>> enumAdapters();
    
  private:
    
    Rc<vk::LibraryFn>   m_vkl;
    Rc<vk::InstanceFn>  m_vki;
    
    VkInstance createInstance();
    
    void logNameList(const vk::NameList& names);
    
  };
  
}