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
  struct DxvkBindingInfo {
    VkDescriptorType   type;
    VkShaderStageFlags stages;
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
      const Rc<vk::DeviceFn>& vkd,
            uint32_t          bindingCount,
      const DxvkBindingInfo*  bindingInfos);
    
    ~DxvkBindingLayout();
    
    /**
     * \brief Number of resource bindings
     * \returns Resource binding count
     */
    uint32_t numBindings() const {
      return m_bindings.size();
    }
    
    /**
     * \brief Retrieves binding info
     * 
     * \param [in] binding ID
     * \returns Binding info
     */
    DxvkBindingInfo binding(uint32_t id) const;
    
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
    
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
    
  };
  
}