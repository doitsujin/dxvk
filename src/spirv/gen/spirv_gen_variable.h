#pragma once

#include "../spirv_code_buffer.h"

#include "spirv_gen_id.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V variable generator
   * 
   * Generates global variable declarations.
   */
  class SpirvVariables {
    
  public:
    
    SpirvVariables();
    ~SpirvVariables();
    
    const SpirvCodeBuffer& code() const {
      return m_code;
    }
    
  private:
    
    SpirvCodeBuffer m_code;
    
  };
  
}