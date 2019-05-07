#include "dxso_options.h"

namespace dxvk {

  DxsoOptions::DxsoOptions() {}

  DxsoOptions::DxsoOptions(const Rc<DxvkDevice>& device, const D3D9Options& options) {
    const Rc<DxvkAdapter> adapter = device->adapter();
    const DxvkDeviceInfo& devInfo = adapter->devicePropertiesExt();

    useSubgroupOpsForEarlyDiscard
       = (devInfo.coreSubgroup.subgroupSize >= 4)
      && (devInfo.coreSubgroup.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT)
      && (devInfo.coreSubgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);

    // Disable early discard on RADV due to GPU hangs
    // Disable early discard on Nvidia because it may hurt performance
    if (adapter->matchesDriver(DxvkGpuVendor::Amd,    VK_DRIVER_ID_MESA_RADV_KHR,          0, 0)
     || adapter->matchesDriver(DxvkGpuVendor::Nvidia, VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR, 0, 0))
      useSubgroupOpsForEarlyDiscard = false;
    
    // Apply shader-related options
    applyTristate(useSubgroupOpsForEarlyDiscard, device->config().useEarlyDiscard);

    strictConstantCopies = options.strictConstantCopies;

    strictPow            = options.strictPow;
  }

}