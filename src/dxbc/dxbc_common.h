#pragma once

#include "dxbc_include.h"

namespace dxvk {
  
  /**
   * \brief DXBC Program type
   * 
   * Defines the shader stage that a DXBC
   * module has been compiled form.
   */
  enum class DxbcProgramType : uint16_t {
    PixelShader     = 0,
    VertexShader    = 1,
    GeometryShader  = 2,
    HullShader      = 3,
    DomainShader    = 4,
    ComputeShader   = 5,
  };
  
  
  /**
   * \brief DXBC shader version info
   * 
   * Stores the shader model version
   * as well as the program type.
   */
  class DxbcProgramVersion {
    
  public:
    
    DxbcProgramVersion() { }
    DxbcProgramVersion(
      uint8_t major, uint8_t minor, DxbcProgramType type)
    : m_major(major), m_minor(minor), m_type(type) { }
    
    /**
     * \brief Major version
     * \returns Major version
     */
    uint32_t major() const {
      return m_major;
    }
    
    /**
     * \brief Minor version
     * \returns Minor version
     */
    uint32_t minor() const {
      return m_minor;
    }
    
    /**
     * \brief Program type
     * \returns Program type
     */
    DxbcProgramType type() const {
      return m_type;
    }
    
  private:
    
    uint8_t         m_major = 0;
    uint8_t         m_minor = 0;
    DxbcProgramType m_type  = DxbcProgramType::PixelShader;
    
  };
  
}