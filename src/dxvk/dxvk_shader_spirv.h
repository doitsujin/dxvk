#pragma once

#include <optional>

#include "dxvk_shader.h"

namespace dxvk {

  struct DxvkSpirvDecorations {
    int32_t memberIndex = -1;
    std::optional<uint32_t> location;
    std::optional<uint32_t> index;
    std::optional<uint32_t> component;
    std::optional<uint32_t> set;
    std::optional<uint32_t> binding;
    std::optional<uint32_t> offset;
    std::optional<uint32_t> stride;
    bool patch = false;
    std::optional<spv::BuiltIn> builtIn;
  };


  /**
   * \brief SPIR-V shader
   */
  class DxvkSpirvShader : public DxvkShader {

  public:

    DxvkSpirvShader(
      const DxvkShaderCreateInfo&       info,
            SpirvCodeBuffer&&           spirv);

    ~DxvkSpirvShader();

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
      const DxvkShaderModuleCreateInfo& state) const;

    /**
     * \brief Queries shader binding layout
     * \returns Pipeline layout builder
     */
    DxvkPipelineLayoutBuilder getLayout() const {
      return m_layout;
    }

    /**
     * \brief Dumps SPIR-V binary to a stream
     * \param [in] outputStream Stream to write to
     */
    void dump(std::ostream& outputStream) const;

    /**
     * \brief Retrieves debug name for this shader
     * \returns Shader debug name
     */
    std::string debugName() const;

  private:

    SpirvCompressedBuffer         m_code;
    DxvkPipelineLayoutBuilder     m_layout;

    std::string                   m_debugName;
    uint32_t                      m_pushConstantStructId = 0u;

    std::unordered_multimap<uint32_t, DxvkSpirvDecorations> m_decorations = { };
    std::unordered_map<uint32_t, uint32_t> m_idToOffset = { };

    void gatherIdOffsets(
            SpirvCodeBuffer&          code);

    void gatherMetadata(
            SpirvCodeBuffer&          code);

    void handleIoVariable(
            SpirvCodeBuffer&          code,
      const SpirvInstruction&         type,
            spv::StorageClass         storage,
            uint32_t                  varId,
            int32_t                   member);

    void handleDecoration(
      const SpirvInstruction&         ins,
            uint32_t                  id,
            int32_t                   member,
            uint32_t                  baseArg);

    void handleDebugName(
            SpirvCodeBuffer&          code,
            uint32_t                  stringId);

    const DxvkSpirvDecorations& getDecoration(
            uint32_t                  id,
            int32_t                   member) const;

    uint32_t getComponentCountForType(
            SpirvCodeBuffer&          code,
      const SpirvInstruction&         type,
            spv::BuiltIn              builtIn) const;

    void patchResourceBindingsAndIoLocations(
            SpirvCodeBuffer&            code,
      const DxvkShaderBindingMap*       bindings,
      const DxvkShaderModuleCreateInfo& state) const;

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

    static void patchInputTopology(
            SpirvCodeBuffer&          code,
            VkPrimitiveTopology       topology);

  };

}
