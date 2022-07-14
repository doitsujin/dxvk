#include "../d3d11/d3d11_options.h"

#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions() {

  }


  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device, const D3D11Options& options) {
    const Rc<DxvkAdapter> adapter = device->adapter();

    const DxvkDeviceFeatures& devFeatures = device->features();
    const DxvkDeviceInfo& devInfo = adapter->devicePropertiesExt();

    useDepthClipWorkaround
      = !devFeatures.extDepthClipEnable.depthClipEnable;
    useStorageImageReadWithoutFormat
      = devFeatures.core.features.shaderStorageImageReadWithoutFormat;
    useSubgroupOpsForAtomicCounters
      = (devInfo.vk11.subgroupSupportedStages     & VK_SHADER_STAGE_COMPUTE_BIT)
     && (devInfo.vk11.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    useDemoteToHelperInvocation
      = (devFeatures.vk13.shaderDemoteToHelperInvocation);
    useSubgroupOpsForEarlyDiscard
      = (devInfo.vk11.subgroupSize >= 4)
     && (devInfo.vk11.subgroupSupportedStages     & VK_SHADER_STAGE_FRAGMENT_BIT)
     && (devInfo.vk11.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    
    switch (device->config().useRawSsbo) {
      case Tristate::Auto:  minSsboAlignment = devInfo.core.properties.limits.minStorageBufferOffsetAlignment; break;
      case Tristate::True:  minSsboAlignment =  4u; break;
      case Tristate::False: minSsboAlignment = ~0u; break;
    }
    
    invariantPosition        = options.invariantPosition;
    enableRtOutputNanFixup   = options.enableRtOutputNanFixup;
    zeroInitWorkgroupMemory  = options.zeroInitWorkgroupMemory;
    forceTgsmBarriers        = options.forceTgsmBarriers;
    disableMsaa              = options.disableMsaa;

    // Figure out float control flags to match D3D11 rules
    if (options.floatControls) {
      if (devInfo.vk12.shaderSignedZeroInfNanPreserveFloat32)
        floatControl.set(DxbcFloatControlFlag::PreserveNan32);
      if (devInfo.vk12.shaderSignedZeroInfNanPreserveFloat64)
        floatControl.set(DxbcFloatControlFlag::PreserveNan64);

      if (devInfo.vk12.denormBehaviorIndependence != VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE) {
        if (devInfo.vk12.shaderDenormFlushToZeroFloat32)
          floatControl.set(DxbcFloatControlFlag::DenormFlushToZero32);
        if (devInfo.vk12.shaderDenormPreserveFloat64)
          floatControl.set(DxbcFloatControlFlag::DenormPreserve64);
      }
    }

    if (!devInfo.vk12.shaderSignedZeroInfNanPreserveFloat32)
      enableRtOutputNanFixup = true;
  }
  
}