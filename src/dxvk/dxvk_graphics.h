#pragma once

#include <mutex>
#include <unordered_map>

#include "dxvk_hash.h"
#include "dxvk_shader.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  struct DxvkGraphicsPipelineState {
    VkRenderPass renderPass;
    
    size_t hash() const;
    
    bool operator == (const DxvkGraphicsPipelineState& other) const;
    bool operator != (const DxvkGraphicsPipelineState& other) const;
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
      const Rc<vk::DeviceFn>& vkd,
      const Rc<DxvkShader>&   vs,
      const Rc<DxvkShader>&   tcs,
      const Rc<DxvkShader>&   tes,
      const Rc<DxvkShader>&   gs,
      const Rc<DxvkShader>&   fs);
    ~DxvkGraphicsPipeline();
    
    VkDescriptorSetLayout descriptorSetLayout() const {
      return m_descriptorSetLayout;
    }
    
    VkPipeline getPipelineHandle(
      const DxvkGraphicsPipelineState& state);
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    Rc<DxvkShader>        m_vs;
    Rc<DxvkShader>        m_tcs;
    Rc<DxvkShader>        m_tes;
    Rc<DxvkShader>        m_gs;
    Rc<DxvkShader>        m_fs;
    
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    
    std::mutex m_mutex;
    
    std::unordered_map<
      DxvkGraphicsPipelineState,
      VkPipeline, DxvkHash> m_pipelines;
    
    VkPipeline compilePipeline(
      const DxvkGraphicsPipelineState& state) const;
    
  };
  
}