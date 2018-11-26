#include "../d3d11/d3d11_options.h"

#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions() {

  }


  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device, const D3D11Options& options) {
    const DxvkDeviceFeatures& devFeatures = device->features();
    const DxvkDeviceInfo& devInfo = device->adapter()->devicePropertiesExt();
    
    useStorageImageReadWithoutFormat
      = devFeatures.core.features.shaderStorageImageReadWithoutFormat;
    useSubgroupOpsForEarlyDiscard
      = (devInfo.coreSubgroup.subgroupSize >= 4)
     && (devInfo.coreSubgroup.supportedStages     & VK_SHADER_STAGE_FRAGMENT_BIT)
     && (devInfo.coreSubgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    
    zeroInitWorkgroupMemory = options.zeroInitWorkgroupMemory;
    
    // Disable early discard on AMD due to GPU hangs
    // Disable early discard on Nvidia because it may hurt performance
    auto vendor = DxvkGpuVendor(devInfo.core.properties.vendorID);

    if (vendor == DxvkGpuVendor::Amd
     || vendor == DxvkGpuVendor::Nvidia)
      useSubgroupOpsForEarlyDiscard = false;
  }
  
}