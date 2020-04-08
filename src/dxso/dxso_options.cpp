#include "dxso_options.h"

#include "../d3d9/d3d9_device.h"

namespace dxvk {

  DxsoOptions::DxsoOptions() {}

  DxsoOptions::DxsoOptions(D3D9DeviceEx* pDevice, const D3D9Options& options) {
    const Rc<DxvkDevice> device = pDevice->GetDXVKDevice();

    const Rc<DxvkAdapter> adapter = device->adapter();

    const DxvkDeviceFeatures& devFeatures = device->features();
    const DxvkDeviceInfo& devInfo = adapter->devicePropertiesExt();

    useDemoteToHelperInvocation
      = (devFeatures.extShaderDemoteToHelperInvocation.shaderDemoteToHelperInvocation);

    useSubgroupOpsForEarlyDiscard
       = (devInfo.coreSubgroup.subgroupSize >= 4)
      && (devInfo.coreSubgroup.supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT)
      && (devInfo.coreSubgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);

    // Disable early discard on RADV (with LLVM) due to GPU hangs
    // Disable early discard on Nvidia because it may hurt performance
    bool isRadvAco = std::string(devInfo.core.properties.deviceName).find("RADV/ACO") != std::string::npos;

    if ((adapter->matchesDriver(DxvkGpuVendor::Amd,    VK_DRIVER_ID_MESA_RADV_KHR,          0, 0) && !isRadvAco)
     || (adapter->matchesDriver(DxvkGpuVendor::Nvidia, VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR, 0, 0)))
      useSubgroupOpsForEarlyDiscard = false;
    
    // Apply shader-related options
    applyTristate(useSubgroupOpsForEarlyDiscard, device->config().useEarlyDiscard);

    strictConstantCopies = options.strictConstantCopies;

    strictPow            = options.strictPow;
    d3d9FloatEmulation   = options.d3d9FloatEmulation;

    shaderModel          = options.shaderModel;

    invariantPosition    = options.invariantPosition;

    forceSamplerTypeSpecConstants = options.forceSamplerTypeSpecConstants;

    vertexConstantBufferAsSSBO = pDevice->GetVertexConstantLayout().totalSize() > devInfo.core.properties.limits.maxUniformBufferRange;

    longMad = options.longMad;
  }

}