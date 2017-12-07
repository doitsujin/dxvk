#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_pipelayout.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Resource slot
   * 
   * Describes the type of a single resource
   * binding that a shader can access.
   */
  struct DxvkResourceSlot {
    uint32_t         slot;
    VkDescriptorType type;
  };
  
  
  /**
   * \brief Shader module object
   * 
   * Manages a Vulkan shader module. This will not
   * perform any shader compilation. Instead, the
   * context will create pipeline objects on the
   * fly when executing draw calls.
   */
  class DxvkShaderModule : public RcObject {
    
  public:
    
    DxvkShaderModule(
      const Rc<vk::DeviceFn>&     vkd,
            VkShaderStageFlagBits stage,
      const SpirvCodeBuffer&      code);
    
    ~DxvkShaderModule();
    
    VkShaderModule handle() const {
      return m_module;
    }
    
    VkPipelineShaderStageCreateInfo stageInfo() const;
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    VkShaderStageFlagBits m_stage;
    VkShaderModule        m_module;
    
  };
  
  
  /**
   * \brief Shader object
   * 
   * Stores a SPIR-V shader and information on the
   * bindings that the shader uses. In order to use
   * the shader with a pipeline, a shader module
   * needs to be created from he shader object.
   */
  class DxvkShader : public RcObject {
    
  public:
    
    DxvkShader(
      const Rc<vk::DeviceFn>&       vkd,
            VkShaderStageFlagBits   stage,
            uint32_t                slotCount,
      const DxvkResourceSlot*       slotInfos,
      const SpirvCodeBuffer&        code);
    ~DxvkShader();
    
    /**
     * \brief 
     */
    void defineResourceSlots(
            DxvkDescriptorSlotMapping& mapping) const;
    
    /**
     * \brief Creates a shader module
     * 
     * Maps the binding slot numbers 
     * \param [in] mapping Resource slot mapping
     * \returns The shader module
     */
    Rc<DxvkShaderModule> createShaderModule(
      const DxvkDescriptorSlotMapping& mapping) const;
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    VkShaderStageFlagBits m_stage;
    SpirvCodeBuffer       m_code;
    
    std::vector<DxvkResourceSlot> m_slots;
    
  };
  
}