#pragma once

#include "dxvk_include.h"

#include "./spirv/dxvk_spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Shader module
   * 
   * Manages a Vulkan shader module. This will not
   * perform any sort of shader compilation. Instead,
   * the context will create pipeline objects on the
   * fly when executing draw calls.
   */
  class DxvkShader : public RcObject {
    
  public:
    
    DxvkShader(
      const Rc<vk::DeviceFn>&     vkd,
      const SpirvCodeBuffer&      code);
    ~DxvkShader();
    
    /**
     * \brief Shader module handle
     * \returns Shader module handle
     */
    VkShaderModule handle() const {
      return m_shader;
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    VkShaderModule        m_shader;
    
  };
  
}