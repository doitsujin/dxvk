#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
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
    VkShaderStageFlags stages;  ///< Stages that can use the resource
  };
  
  
  /**
   * \brief Shader interface
   * 
   * Describes shader resource bindings
   * for a graphics or compute pipeline.
   */
  class DxvkBindingLayout : public RcObject {
    
  public:
    
    DxvkBindingLayout(
      const Rc<vk::DeviceFn>&   vkd,
            uint32_t            bindingCount,
      const DxvkDescriptorSlot* bindingInfos);
    
    ~DxvkBindingLayout();
    
    /**
     * \brief Number of resource bindings
     * \returns Resource binding count
     */
    uint32_t bindingCount() const {
      return m_bindingSlots.size();
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