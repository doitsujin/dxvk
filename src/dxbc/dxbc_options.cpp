#include "../d3d11/d3d11_options.h"

#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions() {

  }


  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device, const D3D11Options& options) {
    const Rc<DxvkAdapter> adapter = device->adapter();

    const DxvkDeviceFeatures& devFeatures = device->features();
    const DxvkDeviceInfo& devInfo = adapter->devicePropertiesExt();
    
    useStorageImageReadWithoutFormat
      = devFeatures.core.features.shaderStorageImageReadWithoutFormat;
    useSubgroupOpsForEarlyDiscard
      = (devInfo.coreSubgroup.subgroupSize >= 4)
     && (devInfo.coreSubgroup.supportedStages     & VK_SHADER_STAGE_FRAGMENT_BIT)
     && (devInfo.coreSubgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    useRawSsbo
      = (devInfo.core.properties.limits.minStorageBufferOffsetAlignment <= sizeof(uint32_t));
    
    zeroInitWorkgroupMemory = options.zeroInitWorkgroupMemory;
    
    // Disable early discard on RADV due to GPU hangs
    // Disable early discard on Nvidia because it may hurt performance
    if (adapter->matchesDriver(DxvkGpuVendor::Amd,    VK_DRIVER_ID_MESA_RADV_KHR,          0, 0)
     || adapter->matchesDriver(DxvkGpuVendor::Nvidia, VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR, 0, 0))
      useSubgroupOpsForEarlyDiscard = false;
    
    // Apply shader-related options
    applyTristate(useSubgroupOpsForEarlyDiscard, device->config().useEarlyDiscard);
    applyTristate(useRawSsbo,                    device->config().useRawSsbo);
  }
  
}