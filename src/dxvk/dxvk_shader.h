#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_pipelayout.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief Shader interface slots
   * 
   * Stores a bit mask of which shader
   * interface slots are defined. Used
   * purely for validation purposes.
   */
  struct DxvkInterfaceSlots {
    uint32_t inputSlots  = 0;
    uint32_t outputSlots = 0;
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
      const SpirvCodeBuffer&      code,
      const std::string&          name);
    
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
    
    /**
     * \brief The shader's debug name
     * \returns Debug name
     */
    const std::string& debugName() const {
      return m_debugName;
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    VkShaderStageFlagBits m_stage;
    VkShaderModule        m_module;
    std::string           m_debugName;
    
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
      const DxvkInterfaceSlots&     iface,
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
     * \brief Inter-stage interface slots
     * 
     * Retrieves the input and output
     * registers used by the shader.
     * \returns Shader interface slots
     */
    DxvkInterfaceSlots interfaceSlots() const {
      return m_interface;
    }
    
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
    
    /**
     * \brief Sets the shader's debug name
     * 
     * Debug names may be used by the backend in
     * order to help debug shader compiler issues.
     * \param [in] name The shader's name
     */
    void setDebugName(const std::string& name) {
      m_debugName = name;
    }
    
  private:
    
    VkShaderStageFlagBits m_stage;
    SpirvCodeBuffer       m_code;
    
    std::vector<DxvkResourceSlot> m_slots;
    DxvkInterfaceSlots            m_interface;
    std::string                   m_debugName;
    
  };
  
}