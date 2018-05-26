#pragma once

#include "dxvk_limits.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  constexpr uint32_t MaxNumSpecConstants = 1
    + uint32_t(DxvkSpecConstantId::SpecConstantIdMax)
    - uint32_t(DxvkSpecConstantId::SpecConstantIdMin);
  
  /**
   * \brief Spec costant data
   * 
   * The values are derived from the pipeline
   * state vector so that they can be used by
   * the shaders.
   */
  struct DxvkSpecConstantData {
    uint32_t rasterizerSampleCount;
    VkBool32 activeBindings[MaxNumActiveBindings];
  };
  
  
  /**
   * \brief Spec constant map
   * 
   * Stores the specialization constant map.
   * This can be passed to Vulkan when compiling
   * both graphics and compute pipelines.
   */
  class DxvkSpecConstantMap {
    
  public:
    
    DxvkSpecConstantMap();
    
    /**
     * \brief Map entry count
     * 
     * \param [in] bindingCount Number of active bindings
     * \returns The number of map entries to read
     */
    uint32_t mapEntryCount() const {
      return m_mapEntries.size();
    }
    
    /**
     * \brief Map entry data
     * \returns Map entries
     */
    const VkSpecializationMapEntry* mapEntryData() const {
      return m_mapEntries.data();
    }
    
  private:
    
    std::array<VkSpecializationMapEntry, MaxNumSpecConstants + MaxNumActiveBindings> m_mapEntries;
    
    void setConstantEntry(
            DxvkSpecConstantId  specId,
            uint32_t            offset,
            uint32_t            size);
    
    void setBindingEntry(
            uint32_t            binding);
    
  };
  
  extern DxvkSpecConstantMap g_specConstantMap;
  
}