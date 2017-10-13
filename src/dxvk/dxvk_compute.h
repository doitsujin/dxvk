#pragma once

#include "dxvk_shader.h"

namespace dxvk {
  
  /**
   * \brief Compute pipeline
   * 
   * Stores a pipeline object 
   */
  class DxvkComputePipeline : public RcObject {
    
  public:
    
    DxvkComputePipeline(const Rc<vk::DeviceFn>& vkd);
    ~DxvkComputePipeline();
    
    /**
     * \brief Pipeline handle
     * \returns Pipeline handle
     */
    VkPipeline handle() const {
      return m_pipeline;
    }
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    VkPipeline        m_pipeline;
    
  };
  
}