#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device) {
    const VkPhysicalDeviceProperties devProps    = device->adapter()->deviceProperties();
    const VkPhysicalDeviceFeatures   devFeatures = device->features();
    
    // Apply driver-specific workarounds
    const DxvkGpuVendor vendor = static_cast<DxvkGpuVendor>(devProps.vendorID);
    
    if (vendor == DxvkGpuVendor::Nvidia) {
      // Older versions of the driver expect the
      // coordinate vector to have an extra component
      this->addExtraDrefCoordComponent = true;
      
      // From vkd3d: NMin/NMax/NClamp may crash the driver.
      this->useSimpleMinMaxClamp = true;
    }
    
    // Enable certain features if they are supported by the device
    this->useStorageImageReadWithoutFormat = devFeatures.shaderStorageImageReadWithoutFormat;
  }
  
}