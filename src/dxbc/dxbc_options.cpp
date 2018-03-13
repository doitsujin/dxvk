#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device) {
    const VkPhysicalDeviceProperties deviceProps
      = device->adapter()->deviceProperties();
    
    const DxvkGpuVendor vendor
      = static_cast<DxvkGpuVendor>(deviceProps.vendorID);
    
    if (vendor == DxvkGpuVendor::Nvidia) {
      // The driver expects the coordinate
      // vector to have an extra component
      this->addExtraDrefCoordComponent = true;
      
      // From vkd3d: NMin/NMax/NClamp crash the driver.
      this->useSimpleMinMaxClamp = true;
    }
    
    // Inform the user about which workarounds are enabled
    if (this->addExtraDrefCoordComponent)
      Logger::warn("DxbcOptions: Growing coordinate vector for Dref operations");
    
    if (this->useSimpleMinMaxClamp)
      Logger::warn("DxbcOptions: Using FMin/FMax/FClamp instead of NMin/NMax/NClamp");
  }
  
}