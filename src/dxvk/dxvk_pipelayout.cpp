#include <cstring>

#include "dxvk_pipelayout.h"

namespace dxvk {
  
  DxvkDescriptorSlotMapping:: DxvkDescriptorSlotMapping() { }
  DxvkDescriptorSlotMapping::~DxvkDescriptorSlotMapping() { }
  
  
  void DxvkDescriptorSlotMapping::defineSlot(
          uint32_t              slot,
          VkDescriptorType      type,
          VkImageViewType       view,
          VkShaderStageFlagBits stage) {
    uint32_t bindingId = this->getBindingId(slot);
    
    if (bindingId != InvalidBinding) {
      m_descriptorSlots[bindingId].stages |= stage;
    } else {
      DxvkDescriptorSlot slotInfo;
      slotInfo.slot   = slot;
      slotInfo.type   = type;
      slotInfo.view   = view;
      slotInfo.stages = stage;
      m_descriptorSlots.push_back(slotInfo);
    }
  }
  
  
  uint32_t DxvkDescriptorSlotMapping::getBindingId(uint32_t slot) const {
    // This won't win a performance competition, but the number
    // of bindings used by a shader is usually much smaller than
    // the number of resource slots available to the system.
    for (uint32_t i = 0; i < m_descriptorSlots.size(); i++) {
      if (m_descriptorSlots[i].slot == slot)
        return i;
    }
    
    return InvalidBinding;
  }
  
  
  DxvkPipelineLayout::DxvkPipelineLayout(
    const Rc<vk::DeviceFn>&   vkd,
          uint32_t            bindingCount,
    const DxvkDescriptorSlot* bindingInfos)
  : m_vkd(vkd) {
    
    m_bindingSlots.resize(bindingCount);
    
    for (uint32_t i = 0; i < bindingCount; i++)
      m_bindingSlots[i] = bindingInfos[i];
    
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
      throw DxvkError("DxvkPipelineLayout: Failed to create descriptor set layout");
    
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
      throw DxvkError("DxvkPipelineLayout: Failed to create pipeline layout");
    }
  }
  
  
  DxvkPipelineLayout::~DxvkPipelineLayout() {
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