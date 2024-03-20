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
    useSubgroupOpsForAtomicCounters
      = (devInfo.vk11.subgroupSupportedStages     & VK_SHADER_STAGE_COMPUTE_BIT)
     && (devInfo.vk11.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);

    VkFormatFeatureFlags2 r32Features
      = device->getFormatFeatures(VK_FORMAT_R32_SFLOAT).optimal
      & device->getFormatFeatures(VK_FORMAT_R32_UINT).optimal
      & device->getFormatFeatures(VK_FORMAT_R32_SINT).optimal;

    supportsTypedUavLoadR32 = (r32Features & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT);
    supportsRawAccessChains = device->features().nvRawAccessChains.shaderRawAccessChains;

    switch (device->config().useRawSsbo) {
      case Tristate::Auto:  minSsboAlignment = devInfo.core.properties.limits.minStorageBufferOffsetAlignment; break;
      case Tristate::True:  minSsboAlignment =  4u; break;
      case Tristate::False: minSsboAlignment = ~0u; break;
    }
    
    invariantPosition        = options.invariantPosition;
    zeroInitWorkgroupMemory  = options.zeroInitWorkgroupMemory;
    forceVolatileTgsmAccess  = options.forceVolatileTgsmAccess;
    disableMsaa              = options.disableMsaa;
    forceSampleRateShading   = options.forceSampleRateShading;
    enableSampleShadingInterlock = device->features().extFragmentShaderInterlock.fragmentShaderSampleInterlock;

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
  }
  
}