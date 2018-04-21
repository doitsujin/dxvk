#include "dxbc_util.h"

namespace dxvk {
  
  uint32_t computeResourceSlotId(
          DxbcProgramType shaderStage,
          DxbcBindingType bindingType,
          uint32_t        bindingIndex) {
    // First resource slot index for per-stage resources
    const uint32_t stageOffset = 132 + 158 * uint32_t(shaderStage);
    
    if (shaderStage == DxbcProgramType::ComputeShader) {
      //   0 -  13: Constant buffers
      //  14 -  29: Samplers
      //  30 - 157: Shader resources
      // 158 - 221: Unordered access views
      switch (bindingType) {
        case DxbcBindingType::ConstantBuffer:     return bindingIndex + stageOffset +  0;
        case DxbcBindingType::ImageSampler:       return bindingIndex + stageOffset +  14;
        case DxbcBindingType::ShaderResource:     return bindingIndex + stageOffset +  30;
        case DxbcBindingType::UnorderedAccessView:return bindingIndex + stageOffset + 158;
        case DxbcBindingType::UavCounter:         return bindingIndex + stageOffset + 222;
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    } else {
      // Global resource slots
      //   0 -   3: Stream output buffers
      //   4 -  67: Unordered access views
      //  68 - 131: UAV counter buffers
      // Per-stage resource slots:
      //   0 -  13: Constant buffers
      //  14 -  29: Samplers
      //  30 - 157: Shader resources
      switch (bindingType) {
        case DxbcBindingType::StreamOutputBuffer: return bindingIndex + 0;
        case DxbcBindingType::UnorderedAccessView:return bindingIndex + 4;
        case DxbcBindingType::UavCounter:         return bindingIndex + 68;
        case DxbcBindingType::ConstantBuffer:     return bindingIndex + stageOffset +  0;
        case DxbcBindingType::ImageSampler:       return bindingIndex + stageOffset + 14;
        case DxbcBindingType::ShaderResource:     return bindingIndex + stageOffset + 30;
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