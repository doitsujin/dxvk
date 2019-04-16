#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  uint32_t computeResourceSlotId(
        DxsoProgramType shaderStage,
        DxsoBindingType bindingType,
        uint32_t        bindingIndex) {
    const uint32_t stageOffset = 7 * uint32_t(shaderStage);

    if (shaderStage == DxsoProgramType::VertexShader) {
      switch (bindingType) {
        case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0; // 0 + 3 = 3
        case DxsoBindingType::Image:          return bindingIndex + stageOffset + 3; // 3 + 4 = 7
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }
    else { // Pixel Shader
      switch (bindingType) {
      case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0;  // 0 + 3 = 3
        // The extra sampler here is being reserved for DMAP stuff later on.
      case DxsoBindingType::Image:          return bindingIndex + stageOffset + 3;  // 3 + 17 = 20
      default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }

    return 0;
  }

}