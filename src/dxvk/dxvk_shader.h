#pragma once

#include <optional>
#include <vector>

#include "dxvk_include.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_shader_io.h"

#include "../spirv/spirv_code_buffer.h"
#include "../spirv/spirv_compression.h"
#include "../spirv/spirv_module.h"

namespace dxvk {
  
  class DxvkShader;
  class DxvkShaderModule;
  class DxvkPipelineManager;
  struct DxvkPipelineStats;

  /**
   * \brief Shader compile flags
   */
  enum class DxvkShaderCompileFlag : uint32_t {
    /// Whether to detect and resolve cases of missing
    /// shared memory barriers in compute shaders
    InsertSharedMemoryBarriers  = 0u,
    /// Whether to detect and resolve cases of missing
    /// resource memory barriers in compute shaders
    InsertResourceBarriers      = 1u,
    /// Whether loads from typed read-write resources
    /// require the format of the resource to be R32.
    TypedR32LoadRequiresFormat  = 2u,
    /// Whether to replace all multisampled image
    /// resource bindings with single-sampled variants.
    DisableMsaa                 = 3u,
    /// Whether to enable sample interpolation for all
    /// interpolated shader inputs.
    EnableSampleRateShading     = 4u,
    /// Whether the device supports 16-bit int and float
    /// arithmetic. Effectively enables min16 lowering.
    Supports16BitArithmetic     = 5u,
    /// Whether 16-bit push data is supported. Used to
    /// pack sampler indices in the binding model
    Supports16BitPushData       = 6u,
    /// Whether to lower unsigned int to float conversions.
    /// Needed to work around an Nvidia driver bug.
    LowerItoF                   = 7u,
    /// Whether to manualy clamp the input for float-to-integer
    /// conversions to avoid overflow and get correct NaN behaviour
    LowerFtoI                   = 8u,
    /// Whether to lower sin/cos to a custom approximation.
    /// Used on hardware where the built-in intrinsics are
    /// not accurate enough.
    LowerSinCos                 = 9u,
    /// Whether to clamp non-infinite inputs to f32tof16 in
    /// order to work around issues on drivers that use RTE.
    LowerF32toF16               = 10u,
    /// Whether to lower built-in constant arrays to a regular
    /// constant buffer. The register space and index for this
    /// buffer are defined in the compile options.
    LowerConstantArrays         = 11u,
  };

  using DxvkShaderCompileFlags = Flags<DxvkShaderCompileFlag>;


  /**
   * \brief Shader lowering flags
   *
   * These flags do not affect the internal IR.
   */
  enum class DxvkShaderSpirvFlag : uint32_t {
    /// Whether to export point size.
    ExportPointSize             = 0u,
    /// Whether raw access chains are supported.
    SupportsNvRawAccessChains   = 1u,
    /// Whether signed zero / inf / nan preserve is
    /// supported for the given bit width
    SupportsSzInfNanPreserve16  = 2u,
    SupportsSzInfNanPreserve32  = 3u,
    SupportsSzInfNanPreserve64  = 4u,
    /// Whether rounding to nearest even is supported
    /// for the given bit width
    SupportsRte16               = 5u,
    SupportsRte32               = 6u,
    SupportsRte64               = 7u,
    /// Whether rounding towards zero is supported
    /// for the given bit width
    SupportsRtz16               = 8u,
    SupportsRtz32               = 9u,
    SupportsRtz64               = 10u,
    /// Whether flushing denorms is supported for the
    /// given bit width
    SupportsDenormFlush16       = 11u,
    SupportsDenormFlush32       = 12u,
    SupportsDenormFlush64       = 13u,
    /// Whether preserving denorms is supported for the
    /// given bit width
    SupportsDenormPreserve16    = 14u,
    SupportsDenormPreserve32    = 15u,
    SupportsDenormPreserve64    = 16u,
    /// Whether 16/64-bit rounding and denorm modes can be
    /// set independently of the corresponding 32-bit mode
    IndependentRoundMode        = 17u,
    IndependentDenormMode       = 18u,
    /// Whether float control 2 features are supported
    SupportsFloatControls2      = 19u,
    /// Whether dynamic non-uniform resource indexing is
    /// supported. Only applies to typed resources.
    SupportsResourceIndexing    = 20u,
  };

  using DxvkShaderSpirvFlags = Flags<DxvkShaderSpirvFlag>;


  /**
   * \brief Shader compile options
   *
   * Device-level options to enable certain
   * features or behaviours.
   */
  struct DxvkShaderOptions {
    /// Compile flags
    DxvkShaderCompileFlags flags = 0u;
    /// SPIR-V lowering flags
    DxvkShaderSpirvFlags spirv = 0u;
    /// Maximum uniform buffer size, in bytes. Constant buffer bindings
    /// larger than this will be lowered to a storage buffer.
    uint32_t maxUniformBufferSize = 0u;
    /// Maximum uniform buffer count per shader. Constant buffer
    /// bindigs beyond this will be lowered to a storage buffer.
    /// Negative numbers impose no limit on the number of buffers.
    int32_t maxUniformBufferCount = -1;
    /// Maximum tessellation factor. If 0, tessellation factors
    /// will not be clamped beyond what is set in the shader.
    uint8_t maxTessFactor = 0u;
    /// Global push data offset for rasterizer sample count
    uint8_t sampleCountPushDataOffset = 0u;
    /// Minimum required storage buffer alignment. Buffers
    /// with a smaller guaranteed alignment must be demoted
    /// to typed buffers.
    uint16_t minStorageBufferAlignment = 0u;
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
    ExportsPosition,
    ExportsStencilRef,
    ExportsViewportIndexLayerFromVertexStage,
    ExportsSampleMask,
    UsesFragmentCoverage,
    UsesSparseResidency,
    TessellationPoints,
  };

  using DxvkShaderFlags = Flags<DxvkShaderFlag>;
  
  /**
   * \brief Shader metadata
   */
  struct DxvkShaderMetadata {
    /// Shader stage
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    /// Shader property flags
    DxvkShaderFlags flags = { };
    /// Specialization constant IDs used by the shader
    uint32_t specConstantMask = 0u;
    /// Input variables consumed by the shader
    DxvkShaderIo inputs = { };
    /// Output variables produced by the shader
    DxvkShaderIo outputs = { };
    /// Input primitive topology (for geometry shaders only)
    VkPrimitiveTopology inputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    /// Output primitive topology for geometry or tessellation shaders
    VkPrimitiveTopology outputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    /// Fragment shader input locations to consider for flat shading
    uint32_t flatShadingInputs = 0;
    /// Rasterized stream for geometry shaders, or -1
    int32_t rasterizedStream = 0;
    /// Tess control patch vertex count
    uint32_t patchVertexCount = 0;
    /// Transform feedback vertex strides
    std::array<uint32_t, MaxNumXfbBuffers> xfbStrides = { };
  };


  /**
   * \brief Shader module create info
   */
  struct DxvkShaderLinkage {
    bool fsDualSrcBlend  = false;
    bool fsFlatShading   = false;

    VkPrimitiveTopology inputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;

    VkShaderStageFlagBits prevStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    DxvkShaderIo prevStageOutputs = { };

    std::array<VkComponentMapping, MaxNumRenderTargets> rtSwizzles = { };

    bool eq(const DxvkShaderLinkage& other) const;

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
  class DxvkShader {
    
  public:
    
    DxvkShader();

    virtual ~DxvkShader();

    force_inline void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    force_inline void decRef() {
      if (!(m_refCount.fetch_sub(1u, std::memory_order_acquire) - 1u))
        delete this;
    }

    /**
     * \brief Retrieves shader cookie
     *
     * Unique value identifying the shader object that
     * can be used for look-up purposes.
     * \returns Unique shader cookie
     */
    size_t getCookie() const {
      return m_cookie;
    }

    /**
     * \brief Shader metadata
     * \returns Shader metadata
     */
    const DxvkShaderMetadata& metadata() {
      if (unlikely(!m_metadata))
        m_metadata = getShaderMetadata();

      return *m_metadata;
    }

    /**
     * \brief Tests whether this shader needs to be compiled
     *
     * If pipeline libraries are supported, this will return
     * \c false once the pipeline library is being compiled.
     * \returns \c true if compilation is still needed
     */
    bool needsCompile() const {
      return m_needsCompile.load();
    }

    /**
     * \brief Notifies library compile
     *
     * Called automatically when pipeline compilation begins. Returns
     * the previous state of the compile flag, which will be \c true
     * if compilation is still required, and \c false otherwise.
     */
    bool notifyCompile() {
      return m_needsCompile.exchange(false);
    }

    /**
     * \brief Queries shader binding layout
     * \returns Pipeline layout builder
     */
    virtual DxvkPipelineLayoutBuilder getLayout() = 0;

    /**
     * \brief Queries uncached shader metadata
     *
     * Compiles the shader as necessary.
     * \returns Shader metadata
     */
    virtual DxvkShaderMetadata getShaderMetadata() = 0;

    /**
     * \brief Compiles shader
     *
     * Performs any IR conversions and lowering that may be necessary.
     * Shader metadata will be immediately available afterwards.
     */
    virtual void compile() = 0;

    /**
     * \brief Retrieves SPIR-V code for the given shader
     *
     * Creates the final shader binary with the given binding
     * mapping and pipeline state information.
     * \param [in] bindings Biding map
     * \param [in] linkage Pipeline state info
     * \returns Uncompressed SPIR-V code
     */
    virtual SpirvCodeBuffer getCode(
      const DxvkShaderBindingMap*       bindings,
      const DxvkShaderLinkage*          linkage) = 0;

    /**
     * \brief Dumps SPIR-V shader
     * 
     * Can be used to store the SPIR-V code in a file.
     * \param [in] outputStream Stream to write to 
     */
    virtual void dump(std::ostream& outputStream) = 0;

    /**
     * \brief Retrieves debug name
     * \returns The shader's name
     */
    virtual std::string debugName() = 0;

    /**
     * \brief Get lookup hash for a shader
     *
     * Convenience method that returns \c 0 for a null
     * pointer, and the shader's lookup hash otherwise.
     * \param [in] shader The shader
     * \returns The shader's lookup hash, or 0
     */
    static uint32_t getCookie(const Rc<DxvkShader>& shader) {
      return shader != nullptr ? shader->getCookie() : 0;
    }

    /**
     * \brief Queries shader dump path
     * \returns Shader dump path, or empty string
     */
    static const std::string& getShaderDumpPath();

  private:

    static std::atomic<uint32_t>  s_cookie;

    std::atomic<uint32_t>         m_refCount = { 0u };
    uint32_t                      m_cookie = 0;

    std::atomic<bool>             m_needsCompile = { true };

    std::optional<DxvkShaderMetadata> m_metadata;

  };
  

  /**
   * \brief Shader code collection
   * 
   * Manages shader stage and shader module create structures that can
   * be passed to pipeline creation. Vulkan shader modules are not used,
   * instead we rely on maintenance5 functionality.
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
     * \brief Queries number of shaders in the set
     * \returns Shader count
     */
    uint32_t getShaderCount() const {
      return m_shaders.size();
    }

    /**
     * \brief Queries shader by index
     */
    DxvkShader* getShader(uint32_t index) const {
      return m_shaders[index].ptr();
    }

    /**
     * \brief Queries number of shaders in the set
     * \returns Shader count
     */
    DxvkShader* findShader(VkShaderStageFlagBits stage) const {
      for (const auto& shader : m_shaders) {
        if (shader->metadata().stage == stage)
          return shader.ptr();
      }

      return nullptr;
    }

    /**
     * \brief Adds a shader to the key
     *
     * Shaders must be added in stage order.
     * \param [in] shader Shader to add
     */
    void addShader(Rc<DxvkShader> shader);

    /**
     * \brief Checks for equality
     *
     * \param [in] other Key to compare to
     * \returns \c true if the keys are equal
     */
    bool eq(const DxvkShaderPipelineLibraryKey& other) const;

    /**
     * \brief Computes key hash
     * \returns Key hash
     */
    size_t hash() const;

  private:

    small_vector<Rc<DxvkShader>, 4> m_shaders;

  };


  /**
   * \brief Pipeline library handle
   *
   * Stores a pipeline library handle and the necessary link flags.
   */
  struct DxvkShaderPipelineLibraryHandle {
    VkPipeline              handle;
    VkPipelineCreateFlags2  linkFlags;
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
            DxvkDevice*               device,
            DxvkPipelineManager*      manager,
      const DxvkShaderPipelineLibraryKey& key);

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
     * \returns Vulkan pipeline handle
     */
    DxvkShaderPipelineLibraryHandle acquirePipelineHandle();

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

    DxvkDevice*                     m_device = nullptr;
    DxvkPipelineManager*            m_manager = nullptr;

    DxvkShaderPipelineLibraryKey    m_shaders;

    std::optional<DxvkPipelineBindings> m_layout;

    dxvk::mutex                     m_mutex;
    uint32_t                        m_useCount      = 0u;
    bool                            m_compiledOnce  = false;

    std::optional<DxvkShaderPipelineLibraryHandle> m_pipeline;

    dxvk::mutex                     m_identifierMutex;
    DxvkShaderIdentifierSet         m_identifiers;

    void destroyShaderPipelineLocked();

    DxvkShaderPipelineLibraryHandle compileShaderPipelineLocked();

    DxvkShaderPipelineLibraryHandle compileShaderPipeline(
            VkPipelineCreateFlags2        flags);

    VkPipeline compileVertexShaderPipeline(
      const DxvkShaderStageInfo&          stageInfo,
            VkPipelineCreateFlags2        flags);

    VkPipeline compileFragmentShaderPipeline(
      const DxvkShaderStageInfo&          stageInfo,
            VkPipelineCreateFlags2        flags);

    VkPipeline compileComputeShaderPipeline(
      const DxvkShaderStageInfo&          stageInfo,
            VkPipelineCreateFlags2        flags);

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

    void compileShaders();

    bool canCreatePipelineLibrary() const;

    bool canCreatePipelineLibraryForShader(DxvkShader& shader, bool needsPosition) const;

    bool canUsePipelineCacheControl() const;

  };
  
}
