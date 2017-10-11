#include "dxvk_shader.h"

namespace dxvk {
  
  DxvkShader::DxvkShader(
    const Rc<vk::DeviceFn>&     vkd,
    const SpirvCodeBuffer&      code)
  : m_vkd(vkd) {
    TRACE(this);
    
    VkShaderModuleCreateInfo info;
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = nullptr;
    info.flags    = 0;
    info.codeSize = code.size();
    info.pCode    = code.code();
    
    if (m_vkd->vkCreateShaderModule(m_vkd->device(),
          &info, nullptr, &m_shader) != VK_SUCCESS)
      throw DxvkError("DxvkShader::DxvkShader: Failed to create shader");
  }
  
  
  DxvkShader::~DxvkShader() {
    TRACE(this);
    m_vkd->vkDestroyShaderModule(
      m_vkd->device(), m_shader, nullptr);
  }
  
}