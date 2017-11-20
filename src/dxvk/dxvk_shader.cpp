#include "dxvk_shader.h"

namespace dxvk {
  
  DxvkShader::DxvkShader(
    const Rc<vk::DeviceFn>&     vkd,
          VkShaderStageFlagBits stage,
    const SpirvCodeBuffer&      code)
  : m_vkd(vkd), m_stage(stage) {
    TRACE(this, stage);
    
    VkShaderModuleCreateInfo info;
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = nullptr;
    info.flags    = 0;
    info.codeSize = code.size();
    info.pCode    = code.data();
    
    if (m_vkd->vkCreateShaderModule(m_vkd->device(),
          &info, nullptr, &m_module) != VK_SUCCESS)
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to create shader module");
  }
  
  
  DxvkShader::~DxvkShader() {
    TRACE(this);
    
    m_vkd->vkDestroyShaderModule(
      m_vkd->device(), m_module, nullptr);
  }
  
  
  VkPipelineShaderStageCreateInfo DxvkShader::stageInfo() const {
    VkPipelineShaderStageCreateInfo info;
    
    info.sType                = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = m_stage;
    info.module               = m_module;
    info.pName                = "main";
    info.pSpecializationInfo  = nullptr;
    return info;
  }
  
}