#pragma once

#include <vector>

#include "dxvk_include.h"

#include "./spirv/dxvk_spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Shader resource type
   * 
   * Enumerates the types of resources
   * that can be accessed by shaders.
   */
  enum class DxvkResourceType : uint32_t {
    UniformBuffer = 0x00,
    ImageSampler  = 0x01,
    SampledImage  = 0x02,
    StorageBuffer = 0x03,
  };
  
  
  /**
   * \brief Resource slot
   */
  struct DxvkResourceSlot{
    DxvkResourceType type;
    uint32_t         slot;
  };
  
  
  /**
   * \brief Shader interface
   * 
   * Stores a list of resource bindings in the
   * order they are defined in the shader module.
   */
  class DxvkShaderInterface {
    
  public:
    
    DxvkShaderInterface();
    ~DxvkShaderInterface();
    
    auto size() const { return m_slots.size(); }
    auto data() const { return m_slots.data(); }
    
    void enableResourceSlot(
      const DxvkResourceSlot& slot);
    
  private:
    
    std::vector<DxvkResourceSlot> m_slots;
    
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
      const DxvkShaderInterface&  iface,
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
    DxvkShaderInterface   m_iface;
    VkShaderModule        m_shader;
    
  };
  
}