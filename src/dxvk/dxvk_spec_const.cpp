#include "dxvk_spec_const.h"

#define SET_CONSTANT_ENTRY(specId, member)  \
  this->setConstantEntry(specId,            \
    offsetof(DxvkSpecConstantData, member), \
    sizeof(DxvkSpecConstantData::member))

namespace dxvk {
  
  DxvkSpecConstantMap g_specConstantMap;
  
  DxvkSpecConstantMap::DxvkSpecConstantMap() {
    SET_CONSTANT_ENTRY(DxvkSpecConstantId::RasterizerSampleCount, rasterizerSampleCount);
    SET_CONSTANT_ENTRY(DxvkSpecConstantId::AlphaTestEnable,       alphaTestEnable);
    SET_CONSTANT_ENTRY(DxvkSpecConstantId::AlphaCompareOp,        alphaCompareOp);

    for (uint32_t i = 0; i < MaxNumActiveBindings; i++)
      this->setBindingEntry(i);
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      this->setOutputMappingEntry(i);
  }
  
  
  void DxvkSpecConstantMap::setConstantEntry(
          DxvkSpecConstantId  specId,
          uint32_t            offset,
          uint32_t            size) {
    VkSpecializationMapEntry entry;
    entry.constantID = uint32_t(specId);
    entry.offset     = offset;
    entry.size       = size;
    m_mapEntries[uint32_t(specId) - uint32_t(DxvkSpecConstantId::SpecConstantIdMin)] = entry;
  }


  void DxvkSpecConstantMap::setBindingEntry(
          uint32_t            binding) {
    VkSpecializationMapEntry entry;
    entry.constantID = binding;
    entry.offset     = sizeof(VkBool32) * binding + offsetof(DxvkSpecConstantData, activeBindings);
    entry.size       = sizeof(VkBool32);
    m_mapEntries[MaxNumSpecConstants + binding] = entry;
  }


  void DxvkSpecConstantMap::setOutputMappingEntry(
          uint32_t            output) {
    for (uint32_t i = 0; i < 4; i++) {
      uint32_t constId = 4 * output + i;

      VkSpecializationMapEntry entry;
      entry.constantID = uint32_t(DxvkSpecConstantId::ColorComponentMappings) + constId;
      entry.offset     = sizeof(uint32_t) * constId + offsetof(DxvkSpecConstantData, outputMappings);
      entry.size       = sizeof(uint32_t);
      m_mapEntries[MaxNumSpecConstants + MaxNumActiveBindings + constId] = entry;
    }
  }




  DxvkSpecConstants::DxvkSpecConstants() {

  }


  DxvkSpecConstants::~DxvkSpecConstants() {

  }


  VkSpecializationInfo DxvkSpecConstants::getSpecInfo() const {
    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount = m_map.size();
    specInfo.pMapEntries   = m_map.data();
    specInfo.dataSize      = m_data.size() * sizeof(uint32_t);
    specInfo.pData         = m_data.data();
    return specInfo;
  }


  void DxvkSpecConstants::setAsUint32(uint32_t specId, uint32_t value) {
    uint32_t index = m_data.size();
    m_data.push_back(value);

    VkSpecializationMapEntry mapEntry;
    mapEntry.constantID = specId;
    mapEntry.offset     = sizeof(uint32_t) * index;
    mapEntry.size       = sizeof(uint32_t);
    m_map.push_back(mapEntry);
  }
  
}