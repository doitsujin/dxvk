#pragma once

#include "dxvk_shader.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Compute pipeline
   * 
   * Stores a compute pipeline object and the corresponding
   * pipeline layout. Unlike graphics pipelines, compute
   * pipelines do not need to be recompiled against any sort
   * of pipeline state.
   */
  class DxvkComputePipeline : public DxvkResource {
    
  public:
    
    DxvkComputePipeline(
      const Rc<vk::DeviceFn>& vkd,
      const Rc<DxvkShader>&   shader);
    ~DxvkComputePipeline();
    
    /**
     * \brief Descriptor set layout
     * 
     * The descriptor set layout for this pipeline.
     * Use this to allocate new descriptor sets.
     * \returns The descriptor set layout
     */
    VkDescriptorSetLayout descriptorSetLayout() const {
      return m_descriptorSetLayout;
    }
    
    /**
     * \brief Pipeline layout layout
     * 
     * The pipeline layout for this pipeline.
     * Use this to bind descriptor sets.
     * \returns The descriptor set layout
     */
    VkPipelineLayout pipelineLayout() const {
      return m_pipelineLayout;
    }
    
    /**
     * \brief Pipeline handle
     * \returns Pipeline handle
     */
    VkPipeline getPipelineHandle() const {
      return m_pipeline;
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    Rc<DxvkShader>        m_shader;
    
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            m_pipeline            = VK_NULL_HANDLE;
    
    void destroyObjects();
    
  };
  
}