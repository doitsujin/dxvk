#include "dxvk_compute.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
    const Rc<vk::DeviceFn>& vkd,
    const Rc<DxvkShader>&   shader)
  : m_vkd(vkd), m_shader(shader) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // TODO re-implement shader slots and bindings
//     for (uint32_t i = 0; i < shader->slotCount(); i++)
//       bindings.push_back(shader->slotBinding(0, i));
    
    VkDescriptorSetLayoutCreateInfo dlayout;
    dlayout.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlayout.pNext        = nullptr;
    dlayout.flags        = 0;
    dlayout.bindingCount = bindings.size();
    dlayout.pBindings    = bindings.data();
    
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(),
          &dlayout, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to create descriptor set layout");
    
    VkPipelineLayoutCreateInfo playout;
    playout.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    playout.pNext                  = nullptr;
    playout.flags                  = 0;
    playout.setLayoutCount         = 1;
    playout.pSetLayouts            = &m_descriptorSetLayout;
    playout.pushConstantRangeCount = 0;
    playout.pPushConstantRanges    = nullptr;
    
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(),
          &playout, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
      this->destroyObjects();
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to create pipeline layout");
    }
    
    VkComputePipelineCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = m_shader->stageInfo();
    info.layout               = m_pipelineLayout;
    info.basePipelineHandle   = VK_NULL_HANDLE;
    info.basePipelineIndex    = 0;
    
    if (m_vkd->vkCreateComputePipelines(m_vkd->device(),
          VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline) != VK_SUCCESS) {
      this->destroyObjects();
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to compile pipeline");
    }
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    this->destroyObjects();
  }
  
  
  void DxvkComputePipeline::destroyObjects() {
    if (m_pipeline != VK_NULL_HANDLE)
      m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeline, nullptr);
    
    if (m_pipelineLayout != VK_NULL_HANDLE)
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), m_pipelineLayout, nullptr);
    
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
      m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), m_descriptorSetLayout, nullptr);
  }
  
}