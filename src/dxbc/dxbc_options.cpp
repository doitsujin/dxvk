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

    supportsTypedUavLoadR32 = true;
    supportsTypedUavLoadExtended = true;

    static const std::array<std::pair<VkFormat, bool>, 18> s_typedUavFormats = {
      std::make_pair(VK_FORMAT_R32_SFLOAT, false),
      std::make_pair(VK_FORMAT_R32_UINT, false),
      std::make_pair(VK_FORMAT_R32_SINT, false),
      std::make_pair(VK_FORMAT_R32G32B32A32_SFLOAT, true),
      std::make_pair(VK_FORMAT_R32G32B32A32_UINT, true),
      std::make_pair(VK_FORMAT_R32G32B32A32_SINT, true),
      std::make_pair(VK_FORMAT_R16G16B16A16_SFLOAT, true),
      std::make_pair(VK_FORMAT_R16G16B16A16_UINT, true),
      std::make_pair(VK_FORMAT_R16G16B16A16_SINT, true),
      std::make_pair(VK_FORMAT_R8G8B8A8_UNORM, true),
      std::make_pair(VK_FORMAT_R8G8B8A8_UINT, true),
      std::make_pair(VK_FORMAT_R8G8B8A8_SINT, true),
      std::make_pair(VK_FORMAT_R16_SFLOAT, true),
      std::make_pair(VK_FORMAT_R16_UINT, true),
      std::make_pair(VK_FORMAT_R16_SINT, true),
      std::make_pair(VK_FORMAT_R8_UNORM, true),
      std::make_pair(VK_FORMAT_R8_UINT, true),
      std::make_pair(VK_FORMAT_R8_SINT, true),
    };

    for (const auto& f : s_typedUavFormats) {
      DxvkFormatFeatures features = device->getFormatFeatures(f.first);
      VkFormatFeatureFlags2 imgFeatures = features.optimal | features.linear;

      if (!(imgFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT)) {
        supportsTypedUavLoadR32 &= f.second;
        supportsTypedUavLoadExtended = false;
      }
    }

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