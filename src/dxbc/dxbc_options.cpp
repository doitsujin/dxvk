#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device) {
    const VkPhysicalDeviceProperties deviceProps
      = device->adapter()->deviceProperties();
    
    const DxvkGpuVendor vendor
      = static_cast<DxvkGpuVendor>(deviceProps.vendorID);
    
    if (vendor == DxvkGpuVendor::Nvidia) {
      // From vkd3d: NMin/NMax/NClamp crash the driver.
      this->useSimpleMinMaxClamp = true;
      
      // From vkd3d: Nvidia expects the depth reference
      // value to be packed into the coordinate vector.
      this->packDrefValueIntoCoordinates = true;
    }
    
    // Inform the user about which workarounds are enabled
    if (this->useSimpleMinMaxClamp)
      Logger::warn("DxbcOptions: Using FMin/FMax/FClamp instead of NMin/NMax/NClamp");
    
    if (this->packDrefValueIntoCoordinates)
      Logger::warn("DxbcOptions: Packing depth reference value into coordinate vector");
  }
  
}