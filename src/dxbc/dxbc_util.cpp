#include "dxbc_util.h"

namespace dxvk {
  
  uint32_t computeResourceSlotId(
          DxbcProgramType shaderStage,
          DxbcBindingType bindingType,
          uint32_t        bindingIndex) {
    // First resource slot index for per-stage resources
    const uint32_t stageOffset = 128 + 160 * uint32_t(shaderStage);
    
    if (shaderStage == DxbcProgramType::ComputeShader) {
      //   0 -  15: Constant buffers
      //  16 -  31: Samplers
      //  32 - 159: Shader resources
      // 160 - 223: Unordered access views
      // 224 - 287: UAV counter buffers
      switch (bindingType) {
        case DxbcBindingType::ConstantBuffer:     return bindingIndex + stageOffset +  0;
        case DxbcBindingType::ImageSampler:       return bindingIndex + stageOffset +  16;
        case DxbcBindingType::ShaderResource:     return bindingIndex + stageOffset +  32;
        case DxbcBindingType::UnorderedAccessView:return bindingIndex + stageOffset + 160;
        case DxbcBindingType::UavCounter:         return bindingIndex + stageOffset + 224;
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    } else {
      // Global resource slots
      //   0 -  63: Unordered access views
      //  64 - 128: UAV counter buffers
      // Per-stage resource slots:
      //   0 -  15: Constant buffers
      //  16 -  31: Samplers
      //  32 - 159: Shader resources
      switch (bindingType) {
        case DxbcBindingType::UnorderedAccessView:return bindingIndex + 0;
        case DxbcBindingType::UavCounter:         return bindingIndex + 64;
        case DxbcBindingType::ConstantBuffer:     return bindingIndex + stageOffset +  0;
        case DxbcBindingType::ImageSampler:       return bindingIndex + stageOffset + 16;
        case DxbcBindingType::ShaderResource:     return bindingIndex + stageOffset + 32;
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }
    
    return 0;
  }
  
  uint32_t primitiveVertexCount(DxbcPrimitive primitive) {
    static const std::array<uint32_t, 8> s_vertexCounts = {
       0, // Undefined
       1, // Point
       2, // Line
       3, // Triangle
       0, // Undefined
       0, // Undefined
       4, // Line with adjacency
       6, // Triangle with adjacency
    };
    
    if (primitive >= DxbcPrimitive::Patch1) {
      return static_cast<uint32_t>(primitive)
           - static_cast<uint32_t>(DxbcPrimitive::Patch1);
    } else {
      return s_vertexCounts.at(
        static_cast<uint32_t>(primitive));
    }
  }
  
}