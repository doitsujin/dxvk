#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  size_t DXSOBytecodeLength(const uint32_t* pFunction) {
    const uint32_t* start = reinterpret_cast<const uint32_t*>(pFunction);
    const uint32_t* current = start;

    while (*current != 0x0000ffff) // A token of 0x0000ffff indicates the end of the bytecode.
      current++;

    return size_t(current - start) * sizeof(uint32_t);
  }

  uint32_t computeResourceSlotId(
        DxsoProgramType shaderStage,
        DxsoBindingType bindingType,
        uint32_t        bindingIndex) {
    const uint32_t stageOffset = 10 * uint32_t(shaderStage);

    if (shaderStage == DxsoProgramType::VertexShader) {
      switch (bindingType) {
        case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0; // 0 + 2 = 2
        case DxsoBindingType::ImageSampler:   return bindingIndex + stageOffset + 2; // 2 + 4 = 6
        case DxsoBindingType::Image:          return bindingIndex + stageOffset + 6; // 6 + 4 = 10
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }
    else { // Pixel Shader
      switch (bindingType) {
      case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0;  // 0 + 1 = 1
        // The extra sampler here is being reserved for DMAP stuff later on.
      case DxsoBindingType::ImageSampler:   return bindingIndex + stageOffset + 1;  // 1 + 17 = 18
      case DxsoBindingType::Image:          return bindingIndex + stageOffset + 18; // 18 + 17 = 35
      default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }

    return 0;
  }

}