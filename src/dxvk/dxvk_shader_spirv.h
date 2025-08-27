#pragma once

#include "dxvk_shader.h"

namespace dxvk {

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

  private:

    struct BindingOffsets {
      uint32_t bindingIndex = 0u;
      uint32_t bindingOffset = 0u;
      uint32_t setIndex = 0u;
      uint32_t setOffset = 0u;
    };

    struct PushDataOffsets {
      uint32_t codeOffset = 0u;
      uint32_t pushOffset = 0u;
    };

    SpirvCompressedBuffer         m_code;

    size_t                        m_o1IdxOffset = 0;
    size_t                        m_o1LocOffset = 0;

    std::vector<BindingOffsets>   m_bindingOffsets;
    std::vector<PushDataOffsets>  m_pushDataOffsets;

    DxvkPipelineLayoutBuilder     m_layout;

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
