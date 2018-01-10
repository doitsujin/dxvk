#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_pipelayout.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {
  
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
    
    /**
     * \brief Shader module handle
     * \returns Shader module handle
     */
    VkShaderModule handle() const {
      return m_module;
    }
    
    /**
     * \brief Shader stage creation info
     * 
     * \param [in] specInfo Specialization info
     * \returns Shader stage create info
     */
    VkPipelineShaderStageCreateInfo stageInfo(
      const VkSpecializationInfo* specInfo) const;
    
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
            VkShaderStageFlagBits   stage,
            uint32_t                slotCount,
      const DxvkResourceSlot*       slotInfos,
      const SpirvCodeBuffer&        code);
    
    ~DxvkShader();
    
    /**
     * \brief Adds resource slots definitions to a mapping
     * 
     * Used to generate the exact descriptor set layout when
     * compiling a graphics or compute pipeline. Slot indices
     * have to be mapped to actual binding numbers.
     */
    void defineResourceSlots(
            DxvkDescriptorSlotMapping& mapping) const;
    
    /**
     * \brief Creates a shader module
     * 
     * Maps the binding slot numbers 
     * \param [in] vkd Vulkan device functions
     * \param [in] mapping Resource slot mapping
     * \returns The shader module
     */
    Rc<DxvkShaderModule> createShaderModule(
      const Rc<vk::DeviceFn>&          vkd,
      const DxvkDescriptorSlotMapping& mapping) const;
    
    /**
     * \brief Dumps SPIR-V shader
     * 
     * Can be used to store the SPIR-V code in a file.
     * \param [in] outputStream Stream to write to 
     */
    void dump(std::ostream&& outputStream) const;
    
    /**
     * \brief Reads SPIR-V shader
     * 
     * Can be used to replace the compiled SPIR-V code.
     * \param [in] inputStream Stream to read from
     */
    void read(std::istream&& inputStream);
    
  private:
    
    VkShaderStageFlagBits m_stage;
    SpirvCodeBuffer       m_code;
    
    std::vector<DxvkResourceSlot> m_slots;
    
  };
  
}