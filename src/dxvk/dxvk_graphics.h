#pragma once

#include <mutex>
#include <unordered_map>

#include "dxvk_constant_state.h"
#include "dxvk_hash.h"
#include "dxvk_pipeline.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  /**
   * \brief Graphics pipeline state info
   * 
   * Stores all information that is required to create
   * a graphics pipeline, except the shader objects
   * themselves. Also used to identify pipelines using
   * the current pipeline state vector.
   */
  struct DxvkGraphicsPipelineStateInfo {
    Rc<DxvkInputAssemblyState>  inputAssemblyState;
    Rc<DxvkInputLayout>         inputLayout;
    Rc<DxvkRasterizerState>     rasterizerState;
    Rc<DxvkMultisampleState>    multisampleState;
    Rc<DxvkDepthStencilState>   depthStencilState;
    Rc<DxvkBlendState>          blendState;
    
    VkRenderPass                renderPass;
    uint32_t                    viewportCount;
    
    size_t hash() const;
    
    bool operator == (const DxvkGraphicsPipelineStateInfo& other) const;
    bool operator != (const DxvkGraphicsPipelineStateInfo& other) const;
  };
  
  
  /**
   * \brief Graphics pipeline
   * 
   * Stores the pipeline layout as well as methods to
   * recompile the graphics pipeline against a given
   * pipeline state vector.
   */
  class DxvkGraphicsPipeline : public DxvkResource {
    
  public:
    
    DxvkGraphicsPipeline(
      const Rc<vk::DeviceFn>&      vkd,
      const Rc<DxvkBindingLayout>& layout,
      const Rc<DxvkShader>&        vs,
      const Rc<DxvkShader>&        tcs,
      const Rc<DxvkShader>&        tes,
      const Rc<DxvkShader>&        gs,
      const Rc<DxvkShader>&        fs);
    ~DxvkGraphicsPipeline();
    
    /**
     * \brief Descriptor set layout
     * 
     * The descriptor set layout for this pipeline.
     * Use this to allocate new descriptor sets.
     * \returns The descriptor set layout
     */
    VkDescriptorSetLayout descriptorSetLayout() const {
      return m_layout->descriptorSetLayout();
    }
    
    /**
     * \brief Pipeline layout
     * 
     * The pipeline layout for this pipeline.
     * Use this to bind descriptor sets.
     * \returns The descriptor set layout
     */
    VkPipelineLayout pipelineLayout() const {
      return m_layout->pipelineLayout();
    }
    
    /**
     * \brief Pipeline handle
     * \returns Pipeline handle
     */
    VkPipeline getPipelineHandle(
      const DxvkGraphicsPipelineStateInfo& state);
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    Rc<DxvkBindingLayout> m_layout;
    
    Rc<DxvkShader> m_vs;
    Rc<DxvkShader> m_tcs;
    Rc<DxvkShader> m_tes;
    Rc<DxvkShader> m_gs;
    Rc<DxvkShader> m_fs;
    
    std::mutex m_mutex;
    
    std::unordered_map<
      DxvkGraphicsPipelineStateInfo,
      VkPipeline, DxvkHash> m_pipelines;
    
    VkPipeline compilePipeline(
      const DxvkGraphicsPipelineStateInfo& state) const;
    
    void destroyPipelines();
    
  };
  
}