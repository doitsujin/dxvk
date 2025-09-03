#pragma once

#include <atomic>
#include <vector>

#include <dxbc/dxbc_api.h>

#include <ir/ir_builder.h>

#include <ir/passes/ir_pass_lower_io.h>

#include "dxvk_shader.h"

#include "../util/thread.h"

namespace dxvk {

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
    /// Streamout parameters
    small_vector<dxbc_spv::ir::IoXfbInfo, 8u> xfbEntries = { };
  };


  /**
   * \brief IR shader binding to resource index mapping
   *
   * Small interface to determine the DXVK-internal resource
   * binding index for any given resource declaration.
   */
  class DxvkIrResourceMapping {

  public:

    virtual ~DxvkIrResourceMapping();

    virtual uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const = 0;

  };


  /**
   * \brief DXBC-SPIRV IR shader
   */
  class DxvkIrShader : public DxvkShader {

  public:

    DxvkIrShader(
      const DxvkIrShaderCreateInfo&   info,
      const DxvkIrResourceMapping&    mapping,
            dxbc_spv::ir::Builder&&   builder);

    ~DxvkIrShader();

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
    DxvkPipelineLayoutBuilder getLayout() const;

    /**
     * \brief Dumps SPIR-V binary to a stream
     * \param [in] outputStream Stream to write to
     */
    void dump(std::ostream& outputStream);

    /**
     * \brief Retrieves debug name for this shader
     * \returns Shader debug name
     */
    std::string debugName() const;

  private:

    std::string                   m_debugName;

    DxvkIrShaderCreateInfo        m_info;
    DxvkPipelineLayoutBuilder     m_layout;

    dxvk::mutex                   m_mutex;

    std::vector<uint8_t>          m_ir;
    std::atomic<bool>             m_legalizedIr = { false };

    DxvkShaderMetadata            m_metadata = { };

    void legalizeIr();

    void serializeIr(const dxbc_spv::ir::Builder& builder);

    void deserializeIr(dxbc_spv::ir::Builder& builder) const;

    std::string getDebugName(const dxbc_spv::ir::Builder& builder) const;

    void lowerIoBindingModel(dxbc_spv::ir::Builder& builder, const DxvkIrResourceMapping& mapping);

    dxbc_spv::ir::ConvertBufferKindPass::Options getBufferPassOptions() const;

    static dxbc_spv::ir::PrimitiveType convertPrimitiveType(VkPrimitiveTopology topology);

    static dxbc_spv::ir::IoOutputSwizzle convertOutputSwizzle(VkComponentMapping mapping);

    static dxbc_spv::ir::IoOutputComponent convertOutputComponent(VkComponentSwizzle swizzle, dxbc_spv::ir::IoOutputComponent identity);

    static dxbc_spv::ir::ShaderStage convertShaderStage(VkShaderStageFlagBits stage);

    static dxbc_spv::ir::IoMap convertIoMap(const DxvkShaderIo& io, VkShaderStageFlagBits stage);

    static std::optional<dxbc_spv::ir::BuiltIn> convertBuiltIn(spv::BuiltIn builtIn, VkShaderStageFlagBits stage);

  };

}
