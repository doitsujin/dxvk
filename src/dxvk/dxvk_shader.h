#pragma once

#include <vector>

#include "dxvk_include.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_shader_key.h"

#include "../spirv/spirv_code_buffer.h"
#include "../spirv/spirv_compression.h"
#include "../spirv/spirv_module.h"

namespace dxvk {
  
  class DxvkShader;
  class DxvkShaderModule;
  class DxvkPipelineManager;
  struct DxvkPipelineStats;
  
  /**
   * \brief Shader flags
   *
   * Provides extra information about the features
   * used by a shader.
   */
  enum DxvkShaderFlag : uint64_t {
    HasSampleRateShading,
    HasTransformFeedback,
    ExportsPosition,
    ExportsStencilRef,
    ExportsViewportIndexLayerFromVertexStage,
    ExportsSampleMask,
    UsesFragmentCoverage,
    UsesSparseResidency,
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
    /// Flat shading input mask
    uint32_t flatShadingInputs = 0;
    /// Push constant range
    uint32_t pushConstOffset = 0;
    uint32_t pushConstSize = 0;
    /// Uniform buffer data
    uint32_t uniformSize = 0;
    const char* uniformData = nullptr;
    /// Rasterized stream, or -1
    int32_t xfbRasterizedStream = 0;
    /// Tess control patch vertex count
    uint32_t patchVertexCount = 0;
    /// Transform feedback vertex strides
    uint32_t xfbStrides[MaxNumXfbBuffers] = { };
    /// Output primitive topology
    VkPrimitiveTopology outputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
  };


  /**
   * \brief Shader module create info
   */
  struct DxvkShaderModuleCreateInfo {
    bool      fsDualSrcBlend  = false;
    bool      fsFlatShading   = false;
    uint32_t  undefinedInputs = 0;

    std::array<VkComponentMapping, MaxNumRenderTargets> rtSwizzles = { };

    bool eq(const DxvkShaderModuleCreateInfo& other) const;

    size_t hash() const;
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
     * \brief Retrieves spec constant mask
     * \returns Bit mask of used spec constants
     */
    uint32_t getSpecConstantMask() const {
      return m_specConstantMask;
    }

    /**
     * \brief Tests whether this shader needs to be compiled
     *
     * If pipeline libraries are supported, this will return
     * \c false once the pipeline library is being compiled.
     * \returns \c true if compilation is still needed
     */
    bool needsLibraryCompile() const {
      return m_needsLibraryCompile.load();
    }

    /**
     * \brief Notifies library compile
     *
     * Called automatically when pipeline compilation begins.
     * Subsequent calls to \ref needsLibraryCompile will return
     * \c false.
     */
    void notifyLibraryCompile() {
      m_needsLibraryCompile.store(false);
    }

    /**
     * \brief Gets raw code without modification
     */
    SpirvCodeBuffer getRawCode() const {
      return m_code.decompress();
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
     * \brief Tests whether this shader supports pipeline libraries
     *
     * This is true for any vertex, fragment, or compute shader that does not
     * require additional pipeline state to be compiled into something useful.
     * \param [in] standalone Set to \c true to evaluate this in the context
     *    of a single-shader pipeline library, or \c false for a pre-raster
     *    shader library consisting of multiple shader stages.
     * \returns \c true if this shader can be used with pipeline libraries
     */
    bool canUsePipelineLibrary(bool standalone) const;

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

    uint32_t                      m_specConstantMask = 0;
    std::atomic<bool>             m_needsLibraryCompile = { true };

    std::vector<char>             m_uniformData;
    std::vector<BindingOffsets>   m_bindingOffsets;

    DxvkBindingLayout             m_bindings;

    static void eliminateInput(
            SpirvCodeBuffer&          code,
            uint32_t                  location);

    static void emitOutputSwizzles(
            SpirvCodeBuffer&          code,
            uint32_t                  outputMask,
            const VkComponentMapping* swizzles);

    static void emitFlatShadingDeclarations(
            SpirvCodeBuffer&          code,
            uint32_t                  inputMask);

  };
  

  /**
   * \brief Shader module object
   * 
   * Manages a Vulkan shader module. This will not
   * perform any shader compilation. Instead, the
   * context will create pipeline objects on the
   * fly when executing draw calls.
   */
  class DxvkShaderStageInfo {
    
  public:

    DxvkShaderStageInfo(const DxvkDevice* device);

    DxvkShaderStageInfo             (DxvkShaderStageInfo&& other) = delete;
    DxvkShaderStageInfo& operator = (DxvkShaderStageInfo&& other) = delete;

    ~DxvkShaderStageInfo();

    /**
     * \brief Counts shader stages
     * \returns Shader stage count
     */
    uint32_t getStageCount() const {
      return m_stageCount;
    }

    /**
     * \brief Queries shader stage infos
     * \returns Pointer to shader stage infos
     */
    const VkPipelineShaderStageCreateInfo* getStageInfos() const {
      return m_stageInfos.data();
    }

    /**
     * \brief Adds a shader stage with specialization info
     *
     * \param [in] stage Shader stage
     * \param [in] code SPIR-V code
     * \param [in] specinfo Specialization info
     */
    void addStage(
            VkShaderStageFlagBits   stage,
            SpirvCodeBuffer&&       code,
      const VkSpecializationInfo*   specInfo);

    /**
     * \brief Adds stage using a module identifier
     *
     * \param [in] stage Shader stage
     * \param [in] identifier Shader module identifier
     * \param [in] specinfo Specialization info
     */
    void addStage(
            VkShaderStageFlagBits   stage,
      const VkShaderModuleIdentifierEXT& identifier,
      const VkSpecializationInfo*   specInfo);

  private:

    const DxvkDevice* m_device;

    struct ShaderModuleIdentifier {
      VkPipelineShaderStageModuleIdentifierCreateInfoEXT createInfo;
      std::array<uint8_t, VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT> data;
    };

    union ShaderModuleInfo {
      ShaderModuleIdentifier    moduleIdentifier;
      VkShaderModuleCreateInfo  moduleInfo;
    };

    std::array<SpirvCodeBuffer,                 5>  m_codeBuffers;
    std::array<ShaderModuleInfo,                5>  m_moduleInfos = { };
    std::array<VkPipelineShaderStageCreateInfo, 5>  m_stageInfos  = { };
    uint32_t                                        m_stageCount  = 0;

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

    size_t hash() const {
      return size_t(depthClipEnable);
    }
  };


  /**
   * \brief Shader set
   *
   * Stores a set of shader pointers
   * for use in a pipeline library.
   */
  struct DxvkShaderSet {
    DxvkShader* vs = nullptr;
    DxvkShader* tcs = nullptr;
    DxvkShader* tes = nullptr;
    DxvkShader* gs = nullptr;
    DxvkShader* fs = nullptr;
    DxvkShader* cs = nullptr;
  };


  /**
   * \brief Shader identifer set
   *
   * Stores a set of shader module identifiers
   * for use in a pipeline library.
   */
  struct DxvkShaderIdentifierSet {
    VkShaderModuleIdentifierEXT vs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT tcs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT tes = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT gs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT fs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
    VkShaderModuleIdentifierEXT cs = { VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT };
  };


  /**
   * \brief Shader pipeline library key
   */
  class DxvkShaderPipelineLibraryKey {

  public:

    DxvkShaderPipelineLibraryKey();

    ~DxvkShaderPipelineLibraryKey();

    /**
     * \brief Creates shader set from key
     * \returns Shader set
     */
    DxvkShaderSet getShaderSet() const;

    /**
     * \brief Generates merged binding layout
     * \returns Binding layout
     */
    DxvkBindingLayout getBindings() const;

    /**
     * \brief Adds a shader to the key
     *
     * Shaders must be added in stage order.
     * \param [in] shader Shader to add
     */
    void addShader(
      const Rc<DxvkShader>&               shader);

    /**
     * \brief Checks wether a pipeline library can be created
     * \returns \c true if all added shaders are compatible
     */
    bool canUsePipelineLibrary() const;

    /**
     * \brief Checks for equality
     *
     * \param [in] other Key to compare to
     * \returns \c true if the keys are equal
     */
    bool eq(
      const DxvkShaderPipelineLibraryKey& other) const;

    /**
     * \brief Computes key hash
     * \returns Key hash
     */
    size_t hash() const;

  private:

    uint32_t                      m_shaderCount   = 0;
    VkShaderStageFlags            m_shaderStages  = 0;
    std::array<Rc<DxvkShader>, 4> m_shaders;

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
            DxvkPipelineManager*      manager,
      const DxvkShaderPipelineLibraryKey& key,
      const DxvkBindingLayoutObjects* layout);

    ~DxvkShaderPipelineLibrary();

    /**
     * \brief Queries shader module identifier
     *
     * Can be used to compile an optimized pipeline using the same
     * shader code, but without having to wait for the pipeline
     * library for this shader shader to compile first.
     * \param [in] stage Shader stage to query
     * \returns Shader module identifier
     */
    VkShaderModuleIdentifierEXT getModuleIdentifier(
            VkShaderStageFlagBits                 stage);

    /**
     * \brief Acquires pipeline handle for the given set of arguments
     *
     * Either returns an already compiled pipeline library object, or
     * performs the compilation step if that has not happened yet.
     * Increments the use count by one.
     * \param [in] args Compile arguments
     * \returns Vulkan pipeline handle
     */
    VkPipeline acquirePipelineHandle(
      const DxvkShaderPipelineLibraryCompileArgs& args);

    /**
     * \brief Releases pipeline
     *
     * Decrements the use count by 1. If the use count reaches 0,
     * any previously compiled pipeline library object may be
     * destroyed in order to save memory.
     */
    void releasePipelineHandle();

    /**
     * \brief Compiles the pipeline with default arguments
     *
     * This is meant to be called from a worker thread in
     * order to reduce the amount of work done on the app's
     * main thread.
     */
    void compilePipeline();

  private:

    const DxvkDevice*               m_device;
          DxvkPipelineStats*        m_stats;
          DxvkShaderSet             m_shaders;
    const DxvkBindingLayoutObjects* m_layout;

    dxvk::mutex     m_mutex;
    VkPipeline      m_pipeline             = VK_NULL_HANDLE;
    VkPipeline      m_pipelineNoDepthClip  = VK_NULL_HANDLE;
    uint32_t        m_useCount             = 0u;
    bool            m_compiledOnce         = false;

    dxvk::mutex                 m_identifierMutex;
    DxvkShaderIdentifierSet     m_identifiers;

    void destroyShaderPipelinesLocked();

    VkPipeline compileShaderPipelineLocked(
      const DxvkShaderPipelineLibraryCompileArgs& args);

    VkPipeline compileShaderPipeline(
      const DxvkShaderPipelineLibraryCompileArgs& args,
            VkPipelineCreateFlags                 flags);

    VkPipeline compileVertexShaderPipeline(
      const DxvkShaderPipelineLibraryCompileArgs& args,
      const DxvkShaderStageInfo&          stageInfo,
            VkPipelineCreateFlags         flags);

    VkPipeline compileFragmentShaderPipeline(
      const DxvkShaderStageInfo&          stageInfo,
            VkPipelineCreateFlags         flags);

    VkPipeline compileComputeShaderPipeline(
      const DxvkShaderStageInfo&          stageInfo,
            VkPipelineCreateFlags         flags);

    SpirvCodeBuffer getShaderCode(
            VkShaderStageFlagBits         stage) const;

    void generateModuleIdentifierLocked(
            VkShaderModuleIdentifierEXT*  identifier,
      const SpirvCodeBuffer&              spirvCode);

    VkShaderStageFlags getShaderStages() const;

    DxvkShader* getShader(
            VkShaderStageFlagBits         stage) const;

    VkShaderModuleIdentifierEXT* getShaderIdentifier(
            VkShaderStageFlagBits         stage);

    void notifyLibraryCompile() const;

    bool canUsePipelineCacheControl() const;

  };
  
}