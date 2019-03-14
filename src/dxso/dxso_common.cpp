#include "dxso_common.h"

namespace dxvk {

  VkShaderStageFlagBits DxsoProgramInfo::shaderStage() const {
    switch (m_type) {
      case DxsoProgramType::PixelShader:  return VK_SHADER_STAGE_FRAGMENT_BIT;
      case DxsoProgramType::VertexShader: return VK_SHADER_STAGE_VERTEX_BIT;
    }

    throw DxvkError("DxsoProgramInfo::shaderStage: Unsupported program type");
  }


  spv::ExecutionModel DxsoProgramInfo::executionModel() const {
    switch (m_type) {
      case DxsoProgramType::PixelShader:  return spv::ExecutionModelFragment;
      case DxsoProgramType::VertexShader: return spv::ExecutionModelVertex;
    }

    throw DxvkError("DxsoProgramInfo::executionModel: Unsupported program type");
  }

}