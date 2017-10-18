#include "dxvk_shader.h"

namespace dxvk {
  
  DxvkShader::DxvkShader(
          VkShaderStageFlagBits stage,
          SpirvCodeBuffer&&     code,
          uint32_t              numResourceSlots,
    const DxvkResourceSlot*     resourceSlots)
  : m_stage (stage),
    m_code  (std::move(code)) {
    TRACE(this, stage, numResourceSlots);
    
    for (uint32_t i = 0; i < numResourceSlots; i++)
      m_slots.push_back(resourceSlots[i]);
  }
  
  
  DxvkShader::~DxvkShader() {
    TRACE(this);
  }
  
  
  SpirvCodeBuffer DxvkShader::code(
    uint32_t bindingOffset) const {
    // TODO implement properly
    if (bindingOffset != 0)
      Logger::warn("DxvkShader::code: bindingOffset != 0 not yet supported");
    return m_code;
  }
  
  
  uint32_t DxvkShader::slotCount() const {
    return m_slots.size();
  }
  
  
  DxvkResourceSlot DxvkShader::slot(uint32_t slotId) const {
    return m_slots.at(slotId);
  }
  
  
  VkDescriptorSetLayoutBinding DxvkShader::slotBinding(
          uint32_t              slotId,
          uint32_t              bindingOffset) const {
    auto dtype = static_cast<VkDescriptorType>(m_slots.at(slotId).type);
    
    VkDescriptorSetLayoutBinding info;
    info.binding            = bindingOffset + slotId;
    info.descriptorType     = dtype;
    info.descriptorCount    = 1;
    info.stageFlags         = m_stage;
    info.pImmutableSamplers = nullptr;
    return info;
  }
  
}