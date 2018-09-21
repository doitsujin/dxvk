#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_shader_key.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {
  
  class DxvkShader;
  class DxvkShaderModule;
  
  /**
   * \brief Built-in specialization constants
   * 
   * These specialization constants allow the SPIR-V
   * shaders to access some pipeline state like D3D
   * shaders do. They need to be filled in by the
   * implementation at pipeline compilation time.
   */
  enum class DxvkSpecConstantId : uint32_t {
    /// Special constant ranges that do not count
    /// towards the spec constant min/max values
    ColorComponentMappings      = MaxNumResourceSlots,

    // Specialization constants for pipeline state
    SpecConstantRangeStart      = ColorComponentMappings + MaxNumRenderTargets * 4,
    RasterizerSampleCount       = SpecConstantRangeStart + 0,

    /// Lowest and highest known spec constant IDs
    SpecConstantIdMin           = RasterizerSampleCount,
    SpecConstantIdMax           = RasterizerSampleCount,
  };
  
  
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
   * \brief Shader constants
   * 
   * Each shader can have constant data associated
   * with it, which needs to be copied to a uniform
   * buffer. The client API must then bind that buffer
   * to an API-specific buffer binding when using the
   * shader for rendering.
   */
  class DxvkShaderConstData {

  public:

    DxvkShaderConstData();
    DxvkShaderConstData(
            size_t                dwordCount,
      const uint32_t*             dwordArray);

    DxvkShaderConstData             (DxvkShaderConstData&& other);
    DxvkShaderConstData& operator = (DxvkShaderConstData&& other);

    ~DxvkShaderConstData();

    const uint32_t* data() const {
      return m_data;
    }

    size_t sizeInBytes() const {
      return m_size * sizeof(uint32_t);
    }

  private:

    size_t    m_size = 0;
    uint32_t* m_data = nullptr;

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
      const SpirvCodeBuffer&        code,
            DxvkShaderConstData&&   constData);
    
    ~DxvkShader();
    
    /**
     * \brief Shader stage
     * \returns Shader stage
     */
    VkShaderStageFlagBits stage() const {
      return m_stage;
    }
    
    /**
     * \brief Checks whether a capability is enabled
     * 
     * If the shader contains an \c OpCapability
     * instruction with the given capability, it
     * is considered enabled. This may be required
     * to correctly set up certain pipeline states.
     * \param [in] cap The capability to check
     * \returns \c true if \c cap is enabled
     */
    bool hasCapability(spv::Capability cap);
    
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
      const DxvkDescriptorSlotMapping& mapping);
    
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
     * \brief Shader constant data
     * 
     * Returns a read-only reference to the 
     * constant data associated with this
     * shader object.
     * \returns Shader constant data
     */
    const DxvkShaderConstData& shaderConstants() const {
      return m_constData;
    }
    
    /**
     * \brief Dumps SPIR-V shader
     * 
     * Can be used to store the SPIR-V code in a file.
     * \param [in] outputStream Stream to write to 
     */
    void dump(std::ostream& outputStream) const;
    
    /**
     * \brief Sets the shader key
     * \param [in] key Unique key
     */
    void setShaderKey(const DxvkShaderKey& key) {
      m_key = key;
    }

    /**
     * \brief Retrieves shader key
     * \returns The unique shader key
     */
    DxvkShaderKey getShaderKey() const {
      return m_key;
    }
    
    /**
     * \brief Retrieves debug name
     * \returns The shader's name
     */
    std::string debugName() const {
      return m_key.toString();
    }
    
  private:
    
    VkShaderStageFlagBits m_stage;
    SpirvCodeBuffer       m_code;
    
    std::vector<DxvkResourceSlot> m_slots;
    std::vector<size_t>           m_idOffsets;
    DxvkInterfaceSlots            m_interface;
    DxvkShaderConstData           m_constData;
    DxvkShaderKey                 m_key;
    
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
      const Rc<DxvkShader>&       shader,
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
    
    /**
     * \brief Shader object
     * \returns The shader
     */
    Rc<DxvkShader> shader() const {
      return m_shader;
    }

    /**
     * \brief Retrieves shader key
     * \returns Unique shader key
     */
    DxvkShaderKey getShaderKey() const {
      return m_shader->getShaderKey();
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    Rc<DxvkShader>        m_shader;
    VkShaderModule        m_module;
    
  };
  
}