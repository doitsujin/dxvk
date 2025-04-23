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
    forceComputeUavBarriers  = options.forceComputeUavBarriers;
    disableMsaa              = options.disableMsaa;
    forceSampleRateShading   = options.forceSampleRateShading;
    enableSampleShadingInterlock = device->features().extFragmentShaderInterlock.fragmentShaderSampleInterlock;
    supportsTightIcbPacking  = device->features().vk12.uniformBufferStandardLayout;

    // Qcom just breaks for no reason if we export point size,
    // even in an environment where doing so is required.
    needsPointSizeExport = !device->features().khrMaintenance5.maintenance5
                        && !device->adapter()->matchesDriver(VK_DRIVER_ID_QUALCOMM_PROPRIETARY);

    // ANV up to mesa 25.0.2 breaks when we *don't* explicitly write point size
    needsPointSizeExport |= device->adapter()->matchesDriver(VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, Version(), Version(25, 0, 3));

    // Intel's hardware sin/cos is so inaccurate that it causes rendering issues in some games
    sincosEmulation = device->adapter()->matchesDriver(VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA)
                   || device->adapter()->matchesDriver(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS);
    applyTristate(sincosEmulation, options.sincosEmulation);

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
