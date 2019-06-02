#include "dxso_common.h"

namespace dxvk {

  VkShaderStageFlagBits DxsoProgramInfo::shaderStage() const {
    switch (m_type) {
      case DxsoProgramTypes::PixelShader:  return VK_SHADER_STAGE_FRAGMENT_BIT;
      case DxsoProgramTypes::VertexShader: return VK_SHADER_STAGE_VERTEX_BIT;
      default:                             break;
    }

    throw DxvkError("DxsoProgramInfo::shaderStage: Unsupported program type");
  }


  spv::ExecutionModel DxsoProgramInfo::executionModel() const {
    switch (m_type) {
      case DxsoProgramTypes::PixelShader:  return spv::ExecutionModelFragment;
      case DxsoProgramTypes::VertexShader: return spv::ExecutionModelVertex;
      default:                             break;
    }

    throw DxvkError("DxsoProgramInfo::executionModel: Unsupported program type");
  }

}