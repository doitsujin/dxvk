#pragma once

#include <vector>

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Resource slot
   * 
   * Describes the type of a single resource
   * binding that a shader can access.
   */
  struct DxvkResourceSlot {
    uint32_t           slot;
    VkDescriptorType   type;
    VkImageViewType    view;
  };
  
  /**
   * \brief Shader interface binding
   * 
   * Corresponds to a single descriptor binding in
   * Vulkan. DXVK does not use descriptor arrays.
   * Instead, each binding stores one descriptor.
   */
  struct DxvkDescriptorSlot {
    uint32_t           slot;    ///< Resource slot index for the context
    VkDescriptorType   type;    ///< Descriptor type (aka resource type)
    VkImageViewType    view;    ///< Compatible image view type
    VkShaderStageFlags stages;  ///< Stages that can use the resource
  };
  
  
  /**
   * \brief Descriptor slot mapping
   * 
   * Convenience class that generates descriptor slot
   * index to binding index mappings. This is required
   * when generating Vulkan pipeline and descriptor set
   * layouts.
   */
  class DxvkDescriptorSlotMapping {
    constexpr static uint32_t InvalidBinding = 0xFFFFFFFFu;
  public:
    
    DxvkDescriptorSlotMapping();
    ~DxvkDescriptorSlotMapping();
    
    /**
     * \brief Number of descriptor bindings
     * \returns Descriptor binding count
     */
    uint32_t bindingCount() const {
      return m_descriptorSlots.size();
    }
    
    /**
     * \brief Descriptor binding infos
     * \returns Descriptor binding infos
     */
    const DxvkDescriptorSlot* bindingInfos() const {
      return m_descriptorSlots.data();
    }
    
    /**
     * \brief Defines a new slot
     * 
     * Adds a slot to the mapping. If the slot is already
     * defined by another shader stage, this will extend
     * the stage mask by the given stage. Otherwise, an
     * entirely new binding is added.
     * \param [in] slot Resource slot
     * \param [in] type Resource type
     * \param [in] view Image view type
     * \param [in] stage Shader stage
     */
    void defineSlot(
            uint32_t              slot,
            VkDescriptorType      type,
            VkImageViewType       view,
            VkShaderStageFlagBits stage);
    
    /**
     * \brief Gets binding ID for a slot
     * 
     * \param [in] slot Resource slot
     * \returns Binding index, or \c InvalidBinding
     */
    uint32_t getBindingId(
            uint32_t              slot) const;
    
  private:
    
    std::vector<DxvkDescriptorSlot> m_descriptorSlots;
    
  };
  
  
  /**
   * \brief Shader interface
   * 
   * Describes shader resource bindings
   * for a graphics or compute pipeline.
   */
  class DxvkPipelineLayout : public RcObject {
    
  public:
    
    DxvkPipelineLayout(
      const Rc<vk::DeviceFn>&   vkd,
            uint32_t            bindingCount,
      const DxvkDescriptorSlot* bindingInfos);
    
    ~DxvkPipelineLayout();
    
    /**
     * \brief Number of resource bindings
     * \returns Resource binding count
     */
    uint32_t bindingCount() const {
      return m_bindingSlots.size();
    }
    
    /**
     * \brief Resource binding info
     * 
     * \param [in] id Binding index
     * \returns Resource binding info
     */
    const DxvkDescriptorSlot& binding(uint32_t id) const {
      return m_bindingSlots[id];
    }
    
    /**
     * \brief Resource binding info
     * \returns Resource binding info
     */
    const DxvkDescriptorSlot* bindings() const {
      return m_bindingSlots.data();
    }
    
    /**
     * \brief Descriptor set layout handle
     * \returns Descriptor set layout handle
     */
    VkDescriptorSetLayout descriptorSetLayout() const {
      return m_descriptorSetLayout;
    }
    
    /**
     * \brief Pipeline layout handle
     * \returns Pipeline layout handle
     */
    VkPipelineLayout pipelineLayout() const {
      return m_pipelineLayout;
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    
    std::vector<DxvkDescriptorSlot> m_bindingSlots;
    
  };
  
}