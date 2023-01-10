#include "dxso_options.h"

#include "../d3d9/d3d9_device.h"

namespace dxvk {

  DxsoOptions::DxsoOptions() {}

  DxsoOptions::DxsoOptions(D3D9DeviceEx* pDevice, const D3D9Options& options) {
    const Rc<DxvkDevice> device = pDevice->GetDXVKDevice();

    const Rc<DxvkAdapter> adapter = device->adapter();

    const DxvkDeviceFeatures& devFeatures = device->features();
    const DxvkDeviceInfo& devInfo = adapter->devicePropertiesExt();

    // Apply shader-related options
    strictConstantCopies = options.strictConstantCopies;

    strictPow            = options.strictPow;
    d3d9FloatEmulation   = options.d3d9FloatEmulation;

    shaderModel          = options.shaderModel;

    invariantPosition    = options.invariantPosition;

    forceSamplerTypeSpecConstants = options.forceSamplerTypeSpecConstants;
    forceSampleRateShading = options.forceSampleRateShading;

    vertexFloatConstantBufferAsSSBO = pDevice->GetVertexConstantLayout().floatSize() > devInfo.core.properties.limits.maxUniformBufferRange;

    longMad = options.longMad;
    robustness2Supported = devFeatures.extRobustness2.robustBufferAccess2;
  }

}