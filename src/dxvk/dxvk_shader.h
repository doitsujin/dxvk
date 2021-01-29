#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_shader_key.h"

#include "../spirv/spirv_code_buffer.h"
#include "../spirv/spirv_compression.h"

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
    SpecConstantRangeStart      = ColorComponentMappings + MaxNumRenderTargets,
    RasterizerSampleCount       = SpecConstantRangeStart + 0,
    FirstPipelineConstant
  };

  /**
   * \brief Shader flags
   *
   * Provides extra information about the features
   * used by a shader.
   */
  enum DxvkShaderFlag : uint64_t {
    HasSampleRateShading,
    HasTransformFeedback,
    ExportsStencilRef,
    ExportsViewportIndexLayerFromVertexStage,
  };

  using DxvkShaderFlags = Flags<DxvkShaderFlag>;
  
  /**
   * \brief Shader interface slots
   * 
   * Stores a bit mask of which shader
   * interface slots are defined. Used
   * purely for validation purposes.
   */
  struct DxvkInterfaceSlots {
    uint32_t inputSlots      = 0;
    uint32_t outputSlots     = 0;
    uint32_t pushConstOffset = 0;
    uint32_t pushConstSize   = 0;
  };


  /**
   * \brief Additional shader options
   * 
   * Contains additional properties that should be
   * taken into account when creating pipelines.
   */
  struct DxvkShaderOptions {
    /// Rasterized stream, or -1
    int32_t rasterizedStream;
    /// Xfb vertex strides
    uint32_t xfbStrides[MaxNumXfbBuffers];
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
   * \brief Shader module create info
   */
  struct DxvkShaderModuleCreateInfo {
    bool      fsDualSrcBlend  = false;
    uint32_t  undefinedInputs = 0;
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
            SpirvCodeBuffer         code,
      const DxvkShaderOptions&      options,
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
     * \brief Retrieves shader flags
     * \returns Shader flags
     */
    DxvkShaderFlags flags() const {
      return m_flags;
    }
    
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
     * \param [in] info Module create info
     * \returns The shader module
     */
    DxvkShaderModule createShaderModule(
      const Rc<vk::DeviceFn>&          vkd,
      const DxvkDescriptorSlotMapping& mapping,
      const DxvkShaderModuleCreateInfo& info);
    
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
     * \brief Shader options
     * \returns Shader options
     */
    DxvkShaderOptions shaderOptions() const {
      return m_options;
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
      m_hash = key.hash();
    }

    /**
     * \brief Retrieves shader key
     * \returns The unique shader key
     */
    DxvkShaderKey getShaderKey() const {
      return m_key;
    }

    /**
     * \brief Get lookup hash
     * 
     * Retrieves a non-unique hash value derived from the
     * shader key which can be used to perform lookups.
     * This is better than relying on the pointer value.
     * \returns Hash value for map lookups
     */
    size_t getHash() const {
      return m_hash;
    }
    
    /**
     * \brief Retrieves debug name
     * \returns The shader's name
     */
    std::string debugName() const {
      return m_key.toString();
    }

    /**
     * \brief Get lookup hash for a shader
     *
     * Convenience method that returns \c 0 for a null
     * pointer, and the shader's lookup hash otherwise.
     * \param [in] shader The shader
     * \returns The shader's lookup hash, or 0
     */
    static size_t getHash(const Rc<DxvkShader>& shader) {
      return shader != nullptr ? shader->getHash() : 0;
    }
    
  private:
    
    VkShaderStageFlagBits m_stage;
    SpirvCompressedBuffer m_code;
    
    std::vector<DxvkResourceSlot> m_slots;
    std::vector<size_t>           m_idOffsets;
    DxvkInterfaceSlots            m_interface;
    DxvkShaderFlags               m_flags;
    DxvkShaderOptions             m_options;
    DxvkShaderConstData           m_constData;
    DxvkShaderKey                 m_key;
    size_t                        m_hash = 0;

    size_t m_o1IdxOffset = 0;
    size_t m_o1LocOffset = 0;

    static void eliminateInput(SpirvCodeBuffer& code, uint32_t location);

  };
  

  /**
   * \brief Shader module object
   * 
   * Manages a Vulkan shader module. This will not
   * perform any shader compilation. Instead, the
   * context will create pipeline objects on the
   * fly when executing draw calls.
   */
  class DxvkShaderModule {
    
  public:

    DxvkShaderModule();

    DxvkShaderModule(DxvkShaderModule&& other);
    
    DxvkShaderModule(
      const Rc<vk::DeviceFn>&     vkd,
      const Rc<DxvkShader>&       shader,
      const SpirvCodeBuffer&      code);
    
    ~DxvkShaderModule();

    DxvkShaderModule& operator = (DxvkShaderModule&& other);
    
    /**
     * \brief Shader stage creation info
     * 
     * \param [in] specInfo Specialization info
     * \returns Shader stage create info
     */
    VkPipelineShaderStageCreateInfo stageInfo(
      const VkSpecializationInfo* specInfo) const {
      VkPipelineShaderStageCreateInfo stage = m_stage;
      stage.pSpecializationInfo = specInfo;
      return stage;
    }
    
    /**
     * \brief Checks whether module is valid
     * \returns \c true if module is valid
     */
    operator bool () const {
      return m_stage.module != VK_NULL_HANDLE;
    }
    
  private:
    
    Rc<vk::DeviceFn>                m_vkd;
    VkPipelineShaderStageCreateInfo m_stage;
    
  };
  
}