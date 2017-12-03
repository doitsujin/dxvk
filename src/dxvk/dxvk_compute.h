#pragma once

#include "dxvk_pipeline.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"

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
      const Rc<vk::DeviceFn>&      vkd,
      const Rc<DxvkBindingLayout>& layout,
      const Rc<DxvkShader>&        cs);
    ~DxvkComputePipeline();
    
    /**
     * \brief Pipeline layout
     * 
     * Stores the pipeline layout and the descriptor set
     * layout, as well as information on the resource
     * slots used by the pipeline.
     * \returns Pipeline layout
     */
    Rc<DxvkBindingLayout> layout() const {
      return m_layout;
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
    Rc<DxvkBindingLayout> m_layout;
    Rc<DxvkShader>        m_cs;
    
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    
  };
  
}