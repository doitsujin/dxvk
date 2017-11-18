#include "dxvk_shader.h"

namespace dxvk {
  
  DxvkShader::DxvkShader(
          VkShaderStageFlagBits stage,
          SpirvCodeBuffer&&     code)
  : m_stage (stage),
    m_code  (std::move(code)) {
    TRACE(this, stage);
  }
  
  
  DxvkShader::~DxvkShader() {
    TRACE(this);
  }
  
}