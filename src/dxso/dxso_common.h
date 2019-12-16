#pragma once

#include "dxso_include.h"

#include <cstdint>

namespace dxvk {

 /**
 * \brief DXSO Program type
 *
 * Defines the shader stage that a DXSO
 * module has been compiled for.
 */
  namespace DxsoProgramTypes {
    enum DxsoProgramType : uint16_t {
      VertexShader = 0,
      PixelShader  = 1,
      Count        = 2,
    };
  }
  using DxsoProgramType = DxsoProgramTypes::DxsoProgramType;

  class DxsoProgramInfo {

  public:

    DxsoProgramInfo() { }
    DxsoProgramInfo(
            DxsoProgramType type,
            uint32_t        minorVersion,
            uint32_t        majorVersion)
      : m_type{ type }
      , m_minorVersion{ minorVersion }
      , m_majorVersion{ majorVersion } {}

    /**
     * \brief Program type
     * \returns Program type
     */
    DxsoProgramType type() const {
      return m_type;
    }

    /**
     * \brief Vulkan shader stage
     *
     * The \c VkShaderStageFlagBits constant
     * that corresponds to the program type.
     * \returns Vulkan shader stage
     */
    VkShaderStageFlagBits shaderStage() const;

    /**
     * \brief SPIR-V execution model
     *
     * The execution model that corresponds
     * to the Vulkan shader stage.
     * \returns SPIR-V execution model
     */
    spv::ExecutionModel executionModel() const;

    /**
     * \brief Minor version
     * \returns The minor version of the shader model.
     */
    uint32_t minorVersion() const {
      return m_minorVersion;
    }

    /**
     * \brief Major version
     * \returns The major version of the shader model.
     */
    uint32_t majorVersion() const {
      return m_majorVersion;
    }

  private:

    DxsoProgramType m_type;

    uint32_t        m_minorVersion;
    uint32_t        m_majorVersion;

  };

}