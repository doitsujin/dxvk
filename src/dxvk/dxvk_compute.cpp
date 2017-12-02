#include "dxvk_compute.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
    const Rc<vk::DeviceFn>&      vkd,
    const Rc<DxvkBindingLayout>& layout,
    const Rc<DxvkShader>&        cs)
  : m_vkd(vkd), m_layout(layout), m_cs(cs) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    VkComputePipelineCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = cs->stageInfo();
    info.layout               = this->pipelineLayout();
    info.basePipelineHandle   = VK_NULL_HANDLE;
    info.basePipelineIndex    = 0;
    
    if (m_vkd->vkCreateComputePipelines(m_vkd->device(),
          VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline) != VK_SUCCESS)
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to compile pipeline");
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    if (m_pipeline != VK_NULL_HANDLE)
      m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeline, nullptr);
  }
  
}