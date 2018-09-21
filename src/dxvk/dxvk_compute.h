#pragma once

#include <vector>

#include "dxvk_bind_mask.h"
#include "dxvk_pipecache.h"
#include "dxvk_pipelayout.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"
#include "dxvk_stats.h"

namespace dxvk {
  
  class DxvkDevice;
  class DxvkPipelineManager;
  
  /**
   * \brief Compute pipeline state info
   */
  struct DxvkComputePipelineStateInfo {
    bool operator == (const DxvkComputePipelineStateInfo& other) const;
    bool operator != (const DxvkComputePipelineStateInfo& other) const;
    
    DxvkBindingMask bsBindingMask;
  };
  
  
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
            DxvkPipelineManager*    pipeMgr,
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
    DxvkPipelineLayout* layout() const {
      return m_layout.ptr();
    }
    
    /**
     * \brief Pipeline handle
     * 
     * \param [in] state Pipeline state
     * \returns Pipeline handle
     */
    VkPipeline getPipelineHandle(
      const DxvkComputePipelineStateInfo& state);
    
  private:
    
    struct PipelineStruct {
      DxvkComputePipelineStateInfo stateVector;
      VkPipeline                   pipeline;
    };
    
    Rc<vk::DeviceFn>        m_vkd;
    DxvkPipelineManager*    m_pipeMgr;
    
    Rc<DxvkPipelineLayout>  m_layout;
    Rc<DxvkShaderModule>    m_cs;
    
    sync::Spinlock              m_mutex;
    std::vector<PipelineStruct> m_pipelines;
    
    VkPipeline m_basePipeline = VK_NULL_HANDLE;
    
    bool findPipeline(
      const DxvkComputePipelineStateInfo& state,
            VkPipeline&                   pipeline) const;
    
    VkPipeline compilePipeline(
      const DxvkComputePipelineStateInfo& state,
            VkPipeline                    baseHandle) const;
    
    void destroyPipelines();
    
  };
  
}