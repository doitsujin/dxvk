#pragma once

#include "../spirv_code_buffer.h"

#include "spirv_gen_id.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V decoration generator
   * 
   * Generates instructions for descriptor
   * bindings and builtin variable decorations.
   */
  class SpirvDecorations {
    
  public:
    
    SpirvDecorations();
    ~SpirvDecorations();
    
    const SpirvCodeBuffer& code() const {
      return m_code;
    }
     
  private:
    
    SpirvCodeBuffer m_code;
    
  };
  
}