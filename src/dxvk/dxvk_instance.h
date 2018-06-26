#pragma once

#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxvk_openvr.h"

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
     * \brief Retrieves an adapter
     * 
     * \param [in] index Adapter index
     * \returns The adapter, or \c nullptr.
     */
    Rc<DxvkAdapter> enumAdapters(
            uint32_t      index) const;
    
    /**
     * \brief Queries extra device extensions
     * 
     * \param [in] adapter The device to query
     * \returns Extra device extensions
     */
    vk::NameSet queryExtraDeviceExtensions(
      const DxvkAdapter* adapter) const;
    
  private:
    
    VrInstance          m_vr;
    
    Rc<vk::LibraryFn>   m_vkl;
    Rc<vk::InstanceFn>  m_vki;

    std::vector<Rc<DxvkAdapter>> m_adapters;
    
    VkInstance createInstance();

    void createAdapters();
    
    void logNameList(const vk::NameList& names);
    
  };
  
}