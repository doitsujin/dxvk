#include "dxvk_shader.h"

namespace dxvk {
  
  DxvkShaderInterface:: DxvkShaderInterface() { }
  DxvkShaderInterface::~DxvkShaderInterface() { }
  
  void DxvkShaderInterface::enableResourceSlot(
    const DxvkResourceSlot& slot) {
    m_slots.push_back(slot);
  }
  
  
  DxvkShader::DxvkShader(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkShaderInterface&  iface,
    const SpirvCodeBuffer&      code)
  : m_vkd(vkd), m_iface(iface) {
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