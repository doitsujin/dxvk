#pragma once

#include <vector>

#include "dxvk_include.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Resource slot
   * 
   * Describes the type of a single resource
   * binding that a shader can access.
   */
  struct DxvkResourceSlot {
    uint32_t         binding;
    VkDescriptorType type;
  };
  
  
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
            VkShaderStageFlagBits stage,
      const SpirvCodeBuffer&      code);
    ~DxvkShader();
    
    VkShaderModule module() const {
      return m_module;
    }
    
    VkPipelineShaderStageCreateInfo stageInfo() const;
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    VkShaderStageFlagBits m_stage;
    VkShaderModule        m_module;
    
  };
  
}