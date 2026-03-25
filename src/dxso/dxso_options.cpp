#include "dxso_options.h"

#include "../d3d9/d3d9_device.h"

namespace dxvk {

  DxsoOptions::DxsoOptions() {}

  DxsoOptions::DxsoOptions(D3D9DeviceEx* pDevice, const D3D9Options& options) {
    const Rc<DxvkDevice> device = pDevice->GetDXVKDevice();
    const DxvkDeviceInfo& devInfo = device->properties();

    // Apply shader-related options
    d3d9FloatEmulation   = options.d3d9FloatEmulation;

    forceSamplerTypeSpecConstants = options.forceSamplerTypeSpecConstants;
    forceSampleRateShading = options.forceSampleRateShading;

    vertexFloatConstantBufferAsSSBO = pDevice->GetVertexConstantLayout().floatSize() > devInfo.core.properties.limits.maxUniformBufferRange;

    sincosEmulation     = device->getShaderCompileOptions().flags.test(DxvkShaderCompileFlag::LowerSinCos);
  }

}
