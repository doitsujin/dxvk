#pragma once

#include "dxvk_include.h"
#include "dxvk_hash.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

  /**
   * \brief Shader I/O variable
   *
   * Stores metadata about a shader-declared I/O var.
   */
  struct DxvkShaderIoVar {
    /// Built-in. If the variable represents a user
    /// varying instead, this will be BuiltInMax.
    spv::BuiltIn builtIn = spv::BuiltInMax;
    /// User varying location
    uint8_t location = 0u;
    /// User varying component index
    uint8_t componentIndex = 0u;
    /// Component count or array size
    uint8_t componentCount = 0u;
    /// Whether the declaration is a patch constant.
    /// Only used in tessellation shaders.
    bool isPatchConstant = false;
    /// Semantic name and index
    uint32_t semanticIndex = 0u;
    std::string semanticName = { };

    bool eq(const DxvkShaderIoVar& other) const {
      return builtIn         == other.builtIn &&
             location        == other.location &&
             componentIndex  == other.componentIndex &&
             componentCount  == other.componentCount &&
             isPatchConstant == other.isPatchConstant &&
             semanticIndex   == other.semanticIndex &&
             semanticName    == other.semanticName;
    }

    size_t hash() const {
      DxvkHashState hash;
      hash.add(uint32_t(builtIn));
      hash.add(uint32_t(location));
      hash.add(uint32_t(componentIndex));
      hash.add(uint32_t(componentCount));
      hash.add(uint32_t(isPatchConstant));
      hash.add(uint32_t(semanticIndex));
      hash.add(bit::fnv1a_hash(semanticName.data(), semanticName.size()));
      return hash;
    }
  };


  /**
   * \brief Shader I/O metadata
   *
   * Collection of all I/O variables declared in a shader.
   */
  class DxvkShaderIo {

  public:

    DxvkShaderIo();

    ~DxvkShaderIo();

    /**
     * \brief Number of I/O variables in collection
     * \returns Number of declared I/O variables
     */
    uint32_t getVarCount() const {
      return uint32_t(m_vars.size());
    }

    /**
     * \brief Queries I/O variable metadata
     *
     * \param [in] index Variable index
     * \returns Info for the given variable
     */
    DxvkShaderIoVar getVar(uint32_t index) const {
      return m_vars[index];
    }

    /**
     * \brief Adds an I/O variable
     *
     * Ensures that variables are ordered for faster,
     * linear-time compatibility checking later.
     * \param [in] var Variable declaration to add
     */
    void add(DxvkShaderIoVar var);

    /**
     * \brief Computes used location mask
     *
     * Useful when determining which render targets or vertex buffer
     * bindings are written or consumed by a shader.
     * \returns Mask of user I/O locations
     */
    uint32_t computeMask() const;

    /**
     * \brief Checks I/O compatibility between shaders
     *
     * \param [in] stage Shader stage that consumes inputs
     * \param [in] inputs Input variables consumed by the shader
     * \param [in] prevStage Previous stage. Ignored for vertex shaders.
     * \param [in] outputs Output variables written by the previous
     *    stage, or vertex buffer bindings in case of vertex shaders.
     * \param [in] matchSemantics Whether to compare shader semantics.
     *    If not set, semantic names will be ignored completely.
     * \returns \c true if all input variables consumed by the given
     *    shader are written by the previous stage, or \c false if any
     *    fix-up is required.
     */
    static bool checkStageCompatibility(
            VkShaderStageFlagBits stage,
      const DxvkShaderIo&         inputs,
            VkShaderStageFlagBits prevStage,
      const DxvkShaderIo&         outputs,
            bool                  matchSemantics);

    /**
     * \brief Computes I/O object for vertex bindings.
     *
     * \param [in] bindingMask Vertex binding mask
     * \returns I/O object corresponding to the given mask
     */
    static DxvkShaderIo forVertexBindings(uint32_t bindingMask);

  private:

    small_vector<DxvkShaderIoVar, 32> m_vars;

    static bool isBuiltInInputGenerated(
            VkShaderStageFlagBits stage,
            VkShaderStageFlagBits prevStage,
            spv::BuiltIn          builtIn);

    static bool orderBefore(
      const DxvkShaderIoVar&      a,
      const DxvkShaderIoVar&      b);

  };

}
