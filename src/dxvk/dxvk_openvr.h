#pragma once

#include "dxvk_include.h"

namespace vr {
  class IVRCompositor;
}

namespace dxvk {
  
  /**
   * \brief OpenVR instance
   * 
   * Loads Initializes OpenVR to provide
   * access to Vulkan extension queries.
   */
  class VrInstance {
    
  public:
    
    VrInstance();
    ~VrInstance();
    
    /**
     * \brief Queries required instance extensions
     * \returns Set of required instance extensions
     */
    vk::NameSet queryInstanceExtensions() const;
    
    /**
     * \brief Queries required device extensions
     * 
     * \param [in] adapter The Vulkan device to query
     * \returns Set of required device extensions
     */
    vk::NameSet queryDeviceExtensions(VkPhysicalDevice adapter) const;
    
  private:
    
    vr::IVRCompositor* m_compositor = nullptr;
    
    static vk::NameSet parseExtensionList(const std::string& str);
    
    static vr::IVRCompositor* getCompositor();
    
  };
  
}