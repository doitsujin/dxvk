#include "dxvk_spec_const.h"

namespace dxvk {
  
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