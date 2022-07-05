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
    FirstPipelineConstant       = 0,
    /// Special constant ranges that do not count
    /// towards the spec constant min/max values
    ColorComponentMappings      = DxvkLimits::MaxNumSpecConstants,
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
    HasSpecConstants,
    ExportsStencilRef,
    ExportsViewportIndexLayerFromVertexStage,
  };

  using DxvkShaderFlags = Flags<DxvkShaderFlag>;
  
  /**
   * \brief Shader info
   */
  struct DxvkShaderCreateInfo {
    /// Shader stage
    VkShaderStageFlagBits stage;
    /// Descriptor info
    uint32_t bindingCount = 0;
    const DxvkBindingInfo* bindings = nullptr;
    /// Input and output register mask
    uint32_t inputMask = 0;
    uint32_t outputMask = 0;
    /// Push constant range
    uint32_t pushConstOffset = 0;
    uint32_t pushConstSize = 0;
    /// Uniform buffer data
    uint32_t uniformSize = 0;
    const char* uniformData = nullptr;
    /// Rasterized stream, or -1
    int32_t xfbRasterizedStream = 0;
    /// Transform feedback vertex strides
    uint32_t xfbStrides[MaxNumXfbBuffers] = { };
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
      const DxvkShaderCreateInfo&   info,
            SpirvCodeBuffer&&       spirv);

    ~DxvkShader();
    
    /**
     * \brief Shader info
     * \returns Shader info
     */
    const DxvkShaderCreateInfo& info() const {
      return m_info;
    }

    /**
     * \brief Retrieves shader flags
     * \returns Shader flags
     */
    DxvkShaderFlags flags() const {
      return m_flags;
    }

    /**
     * \brief Retrieves binding layout
     * \returns Binding layout
     */
    const DxvkBindingLayout& getBindings() const {
      return m_bindings;
    }

    /**
     * \brief Patches code using given info
     *
     * Rewrites binding IDs and potentially fixes up other
     * parts of the code depending on pipeline state.
     * \param [in] layout Biding layout
     * \param [in] state Pipeline state info
     * \returns Uncompressed SPIR-V code buffer
     */
    SpirvCodeBuffer getCode(
      const DxvkBindingLayoutObjects*   layout,
      const DxvkShaderModuleCreateInfo& state) const;
    
    /**
     * \brief Creates a shader module
     *
     * Remaps resource binding and descriptor set
     * numbers to match the given binding layout.
     * \param [in] vkd Vulkan device functions
     * \param [in] layout Binding layout
     * \param [in] info Module create info
     * \returns The shader module
     */
    DxvkShaderModule createShaderModule(
      const Rc<vk::DeviceFn>&           vkd,
      const DxvkBindingLayoutObjects*   layout,
      const DxvkShaderModuleCreateInfo& info);

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

    struct BindingOffsets {
      uint32_t bindingId;
      uint32_t bindingOffset;
      uint32_t setOffset;
    };

    DxvkShaderCreateInfo          m_info;
    SpirvCompressedBuffer         m_code;
    
    DxvkShaderFlags               m_flags;
    DxvkShaderKey                 m_key;
    size_t                        m_hash = 0;

    size_t                        m_o1IdxOffset = 0;
    size_t                        m_o1LocOffset = 0;

    std::vector<char>             m_uniformData;
    std::vector<BindingOffsets>   m_bindingOffsets;

    DxvkBindingLayout             m_bindings;

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


  /**
   * \brief Shader pipeline library compile args
   */
  struct DxvkShaderPipelineLibraryCompileArgs {
    VkBool32 depthClipEnable = VK_TRUE;

    bool operator == (const DxvkShaderPipelineLibraryCompileArgs& other) const {
      return depthClipEnable == other.depthClipEnable;
    }

    bool operator != (const DxvkShaderPipelineLibraryCompileArgs& other) const {
      return !this->operator == (other);
    }
  };


  /**
   * \brief Shader pipeline library
   *
   * Stores a pipeline object for either a complete compute
   * pipeline, a pre-rasterization pipeline library consisting
   * of a single vertex shader, or a fragment shader pipeline
   * library. All state unknown at shader compile time will
   * be made dynamic.
   */
  class DxvkShaderPipelineLibrary {

  public:

    DxvkShaderPipelineLibrary(
      const DxvkDevice*               device,
      const DxvkShader*               shader,
      const DxvkBindingLayoutObjects* layout);

    ~DxvkShaderPipelineLibrary();

    /**
     * \brief Queries pipeline handle for the given set of arguments
     *
     * Either returns an already compiled pipeline library object, or
     * performs the compilation step if that has not happened yet.
     * \param [in] cache Pipeline cache handle
     * \param [in] args Compile arguments
     * \returns Vulkan pipeline handle
     */
    VkPipeline getPipelineHandle(
            VkPipelineCache                       cache,
      const DxvkShaderPipelineLibraryCompileArgs& args);

    /**
     * \brief Compiles the pipeline with default arguments
     *
     * This is meant to be called from a worker thread in
     * order to reduce the amount of work done on the app's
     * main thread.
     * \param [in] cache Pipeline cache handle
     */
    void compilePipeline(VkPipelineCache cache);

  private:

    const DxvkDevice*               m_device;
    const DxvkShader*               m_shader;
    const DxvkBindingLayoutObjects* m_layout;

    dxvk::mutex     m_mutex;
    VkPipeline      m_pipeline             = VK_NULL_HANDLE;
    VkPipeline      m_pipelineNoDepthClip  = VK_NULL_HANDLE;

    VkPipeline compileVertexShaderPipeline(
            VkPipelineCache                       cache,
      const DxvkShaderPipelineLibraryCompileArgs& args);

    VkPipeline compileFragmentShaderPipeline(VkPipelineCache cache);

    VkPipeline compileComputeShaderPipeline(VkPipelineCache cache);

  };
  
}