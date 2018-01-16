#pragma once

#include "dxvk_pipecache.h"
#include "dxvk_pipelayout.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  class DxvkDevice;
  
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
      const DxvkDevice*             device,
      const Rc<DxvkPipelineCache>&  cache,
      const Rc<DxvkShader>&         cs);
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
    
    const DxvkDevice* const m_device;
    const Rc<vk::DeviceFn>  m_vkd;
    
    Rc<DxvkPipelineCache> m_cache;
    Rc<DxvkBindingLayout> m_layout;
    Rc<DxvkShaderModule>  m_cs;
    
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    
    void compilePipeline();
    
  };
  
}