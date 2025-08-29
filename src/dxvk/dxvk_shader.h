#pragma once

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
   * \brief Shader info
   */
  struct DxvkShaderCreateInfo {
    /// Descriptor info
    uint32_t bindingCount = 0;
    const DxvkBindingInfo* bindings = nullptr;
    /// Flat shading input mask
    uint32_t flatShadingInputs = 0;
    /// Push data blocks
    DxvkPushDataBlock sharedPushData;
    DxvkPushDataBlock localPushData;
    /// Descriptor set and binding of global sampler heap
    DxvkShaderBinding samplerHeap;
    /// Rasterized stream, or -1
    int32_t xfbRasterizedStream = 0;
    /// Tess control patch vertex count
    uint32_t patchVertexCount = 0;
  };


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
  struct DxvkShaderModuleCreateInfo {
    bool      fsDualSrcBlend  = false;
    bool      fsFlatShading   = false;
    VkPrimitiveTopology inputTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;

    std::optional<DxvkShaderIo> prevStageOutputs = { };

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
     * \brief Shader metadata
     * \returns Shader metadata
     */
    const DxvkShaderMetadata& metadata() const {
      return m_metadata;
    }

    /**
     * \brief Queries shader binding layout
     * \returns Pipeline layout builder
     */
    virtual DxvkPipelineLayoutBuilder getLayout() const = 0;

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
     * \brief Patches code using given info
     *
     * Rewrites binding IDs and potentially fixes up other
     * parts of the code depending on pipeline state.
     * \param [in] bindings Biding map
     * \param [in] state Pipeline state info
     * \returns Uncompressed SPIR-V code buffer
     */
    virtual SpirvCodeBuffer getCode(
      const DxvkShaderBindingMap*       bindings,
      const DxvkShaderModuleCreateInfo& state) const = 0;
    
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
    virtual void dump(std::ostream& outputStream) const = 0;

    /**
     * \brief Get lookup hash
     * 
     * Retrieves a non-unique hash value derived from the
     * shader key which can be used to perform lookups.
     * This is better than relying on the pointer value.
     * \returns Hash value for map lookups
     */
    size_t getCookie() const {
      return m_cookie;
    }
    
    /**
     * \brief Retrieves debug name
     * \returns The shader's name
     */
    virtual std::string debugName() const = 0;

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
    
  private:

    static std::atomic<uint32_t>  s_cookie;

    std::atomic<uint32_t>         m_refCount = { 0u };
    uint32_t                      m_cookie = 0;

  protected:

    DxvkShaderCreateInfo          m_info  = { };
    DxvkShaderMetadata            m_metadata = { };

    std::atomic<bool>             m_needsLibraryCompile = { true };

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
     * \brief Builds merged binding layout
     * \returns Pipeline layout builder
     */
    DxvkPipelineLayoutBuilder getLayout() const;

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

    const DxvkDevice*               m_device;

    DxvkPipelineStats*              m_stats;
    DxvkShaderSet                   m_shaders;

    DxvkPipelineBindings            m_layout;

    dxvk::mutex                     m_mutex;
    DxvkShaderPipelineLibraryHandle m_pipeline      = { VK_NULL_HANDLE, 0 };
    uint32_t                        m_useCount      = 0u;
    bool                            m_compiledOnce  = false;

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

    bool canUsePipelineCacheControl() const;

  };
  
}
