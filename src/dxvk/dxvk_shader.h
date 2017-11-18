#pragma once

#include <vector>

#include "dxvk_include.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Shader resource type
   * 
   * Enumerates the types of resources
   * that can be accessed by shaders.
   */
  enum class DxvkResourceType : uint32_t {
    ImageSampler       = VK_DESCRIPTOR_TYPE_SAMPLER,
    SampledImage       = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    StorageImage       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    UniformBuffer      = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    StorageBuffer      = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    UniformTexelBuffer = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
    StorageTexelBuffer = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
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
            VkShaderStageFlagBits stage,
            SpirvCodeBuffer&&     code);
    ~DxvkShader();
    
    /**
     * \brief Retrieves shader code
     * \returns Shader code buffer
     */
    const SpirvCodeBuffer& code() const {
      return m_code;
    }
    
  private:
    
    VkShaderStageFlagBits m_stage;
    SpirvCodeBuffer       m_code;
    
  };
  
}