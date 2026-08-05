#pragma once

#include <array>
#include <atomic>
#include <string>
#include <vector>

#include <dxbc/dxbc_api.h>

#include <ir/ir_builder.h>

#include <ir/passes/ir_pass_lower_io.h>

#include "dxvk_shader.h"

#include "../util/thread.h"

namespace dxvk {

  /**
   * \brief One pass-through IO register for the amplification GS
   *
   * Caught once, at VS creation time, from the same output-signature
   * walk that resolves NV custom semantics - never the NV interop
   * registers themselves (position-view family, viewport masks), only
   * the plain registers a rasterizer/PS reads.
   */
  struct DxvkNvPassthroughIoEntry {
    uint32_t                 regIndex      = 0u;
    dxbc_spv::ir::BasicType  type          = dxbc_spv::ir::BasicType();
    std::string              semanticName  = { };
    uint32_t                 semanticIndex = 0u;
    /// First written component. Two signature entries can share a register
    /// with different write masks; both need this to be declared correctly.
    uint32_t                 component     = 0u;
  };


  /**
   * \brief NVAPI multi-view semantic mapping
   *
   * Output registers resolved from the DXBC output signature of a shader
   * created through the NVAPI extended entry points (dxvk-nvapi interop).
   * View 0's position is SV_POSITION; views 1-3 ride the
   * NV_POSITION_VIEW_{1,2,3}_SEMANTIC outputs. Register indices are -1
   * when the corresponding output is not present. Trivially copyable, and
   * hashed by raw bytes: hash() and eq() operate on the whole struct, so
   * the static_assert below pins the layout.
   */
  struct DxvkNvMultiviewInfo {
    std::array<int32_t, 3> positionViewReg = { -1, -1, -1 };
    /// Data type of each register above, the index alone isn't enough
    /// to declare a matching GS input for it.
    std::array<dxbc_spv::ir::BasicType, 3> positionViewType = { };
    int32_t viewportMaskReg = -1;
    int32_t viewportMask2Reg = -1;
    /// Data type of the two registers above, same reasoning as
    /// positionViewType.
    std::array<dxbc_spv::ir::BasicType, 2> viewportMaskType = { };
    uint32_t useViewportMask = 0u;

    bool enabled() const {
      return positionViewReg[0] >= 0 || viewportMaskReg >= 0;
    }
  };

  static_assert(sizeof(DxvkNvMultiviewInfo) == 32u);


  /**
   * \brief IR shader properties
   *
   * Stores some metadata that cannot be inferred from
   * the IR, as well as some binding model mappings.
   */
  struct DxvkIrShaderCreateInfo {
    /// Shader compile options
    DxvkShaderOptions options;
    /// Mask of user input locations to enable flat shading for
    uint32_t flatShadingInputs = 0u;
    /// Rasterized geometry stream
    int32_t rasterizedStream = 0;
    /// NVAPI multi-view semantic mapping (dxvk-nvapi interop)
    DxvkNvMultiviewInfo nvMultiview = { };
    /// Streamout parameters
    small_vector<dxbc_spv::ir::IoXfbInfo, 8u> xfbEntries = { };

    size_t hash() const;

    bool eq(const DxvkIrShaderCreateInfo& other) const;
  };

  /**
   * \brief Raw shader binary for dxbc-spirv
   *
   * Performs the initial shader conversion and provides a method for
   * the shader implementation to map resource registers to DXVK bindings.
   */
  class DxvkIrShaderConverter {

  public:

    void incRef() {
      m_useCount.fetch_add(1u);
    }

    void decRef() {
      if (m_useCount.fetch_sub(1u) == 1u)
        delete this;
    }

    virtual ~DxvkIrShaderConverter();

    /**
     * \brief Performs initial shader conversion
     * \param [out] builder IR builder
     */
    virtual void convertShader(
            dxbc_spv::ir::Builder&    builder) = 0;

    /**
     * \brief Maps IR binding to internal resource index
     *
     * \param [in] stage Shader stage
     * \param [in] type Descriptor type
     * \param [in] regSpace Register space
     * \param [in] regIndex Register index
     */
    virtual uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const = 0;

    /**
     * \brief Dumps source to the given output file
     * \param [in] file Output stream
     */
    virtual void dumpSource(const std::string& path) const = 0;

    /**
     * \brief Queries shader debug name
     * \returns Shader debug name
     */
    virtual std::string getDebugName() const = 0;

  private:

    std::atomic<uint32_t> m_useCount = { };

  };


  /**
   * \brief DXBC-SPIRV IR shader
   */
  class DxvkIrShader : public DxvkShader {

  public:

    // Arbitrarily chosen number that we know we won't use anywhere else
    static constexpr uint32_t SpecDataSet = 7u;

    DxvkIrShader(
      const DxvkIrShaderCreateInfo&   info,
            Rc<DxvkIrShaderConverter> shader);

    DxvkIrShader(
            std::string               name,
      const DxvkIrShaderCreateInfo&   info,
            DxvkShaderMetadata        metadata,
            DxvkPipelineLayoutBuilder layout,
            std::vector<uint8_t>      ir);

    ~DxvkIrShader();

    /**
     * \brief Queries shader create info
     * \returns Shader create info
     */
    DxvkIrShaderCreateInfo getShaderCreateInfo() const {
      return m_info;
    }

    /**
     * \brief Queries shader metadata
     *
     * Compiles the shader on demand.
     * \returns Shader metadata
     */
    DxvkShaderMetadata getShaderMetadata();

    /**
     * \brief Compiles shader to internal IR
     */
    void compile();

    /**
     * \brief Patches code using given info
     *
     * Rewrites binding IDs and potentially fixes up other
     * parts of the code depending on pipeline state.
     * \param [in] bindings Biding map
     * \param [in] state Pipeline state info
     * \returns Uncompressed SPIR-V code buffer
     */
    SpirvCodeBuffer getCode(
      const DxvkShaderBindingMap*       bindings,
      const DxvkShaderLinkage*          linkage);

    /**
     * \brief Queries shader binding layout
     * \returns Pipeline layout builder
     */
    DxvkPipelineLayoutBuilder getLayout();

    /**
     * \brief Dumps SPIR-V binary to a stream
     * \param [in] outputStream Stream to write to
     */
    void dump(std::ostream& outputStream);

    /**
     * \brief Queries serialized IR
     */
    std::pair<const uint8_t*, size_t> getSerializedIr();

    /**
     * \brief Retrieves debug name for this shader
     * \returns Shader debug name
     */
    std::string debugName();

  private:

    Rc<DxvkIrShaderConverter>     m_baseIr;
    std::string                   m_debugName;

    DxvkIrShaderCreateInfo        m_info;
    DxvkPipelineLayoutBuilder     m_layout;

    dxvk::mutex                   m_mutex;

    std::vector<uint8_t>          m_ir;
    std::atomic<bool>             m_convertedIr = { false };

    DxvkShaderMetadata            m_metadata = { };

    void convertIr(const char* reason);

    void convertShader();

    void serializeIr(const dxbc_spv::ir::Builder& builder);

    void deserializeIr(dxbc_spv::ir::Builder& builder) const;

    void dumpSource(const std::string& dumpPath);

    void dumpSpv(const std::string& dumpPath);

    static dxbc_spv::ir::PrimitiveType convertPrimitiveType(VkPrimitiveTopology topology);

    static dxbc_spv::ir::IoOutputSwizzle convertOutputSwizzle(VkComponentMapping mapping);

    static dxbc_spv::ir::IoOutputComponent convertOutputComponent(VkComponentSwizzle swizzle, dxbc_spv::ir::IoOutputComponent identity);

    static dxbc_spv::ir::ShaderStage convertShaderStage(VkShaderStageFlagBits stage);

    static dxbc_spv::ir::IoMap convertIoMap(const DxvkShaderIo& io, VkShaderStageFlagBits stage);

    static std::optional<dxbc_spv::ir::BuiltIn> convertBuiltIn(spv::BuiltIn builtIn, VkShaderStageFlagBits stage);

  };

}
