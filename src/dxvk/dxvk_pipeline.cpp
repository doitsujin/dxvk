#include <cstring>

#include "dxvk_pipeline.h"

namespace dxvk {
  
  DxvkBindingLayout::DxvkBindingLayout(
    const Rc<vk::DeviceFn>&   vkd,
          uint32_t            bindingCount,
    const DxvkDescriptorSlot* bindingInfos)
  : m_vkd(vkd) {
    
    m_bindingSlots.resize(bindingCount);
    std::memcpy(m_bindingSlots.data(), bindingInfos,
      bindingCount * sizeof(DxvkDescriptorSlot));
    
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    for (uint32_t i = 0; i < bindingCount; i++) {
      VkDescriptorSetLayoutBinding binding;
      binding.binding            = i;
      binding.descriptorType     = bindingInfos[i].type;
      binding.descriptorCount    = 1;
      binding.stageFlags         = bindingInfos[i].stages;
      binding.pImmutableSamplers = nullptr;
      bindings.push_back(binding);
    }
    
    VkDescriptorSetLayoutCreateInfo dsetInfo;
    dsetInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsetInfo.pNext        = nullptr;
    dsetInfo.flags        = 0;
    dsetInfo.bindingCount = bindings.size();
    dsetInfo.pBindings    = bindings.data();
    
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(),
          &dsetInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkBindingLayout: Failed to create descriptor set layout");
    
    VkPipelineLayoutCreateInfo pipeInfo;
    pipeInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeInfo.pNext                  = nullptr;
    pipeInfo.flags                  = 0;
    pipeInfo.setLayoutCount         = 1;
    pipeInfo.pSetLayouts            = &m_descriptorSetLayout;
    pipeInfo.pushConstantRangeCount = 0;
    pipeInfo.pPushConstantRanges    = nullptr;
    
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(),
          &pipeInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
      m_vkd->vkDestroyDescriptorSetLayout(
        m_vkd->device(), m_descriptorSetLayout, nullptr);
      throw DxvkError("DxvkBindingLayout: Failed to create pipeline layout");
    }
  }
  
  
  DxvkBindingLayout::~DxvkBindingLayout() {
    if (m_pipelineLayout != VK_NULL_HANDLE) {
      m_vkd->vkDestroyPipelineLayout(
        m_vkd->device(), m_pipelineLayout, nullptr);
    }
    
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
      m_vkd->vkDestroyDescriptorSetLayout(
        m_vkd->device(), m_descriptorSetLayout, nullptr);
    }
  }
  
}