#pragma once

#include "../spirv_code_buffer.h"

#include "spirv_gen_id.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V constant generator
   * 
   * Provides convenient methods to
   * generate SPIR-V constants.
   */
  class SpirvConstants {
    
  public:
    
    SpirvConstants();
    ~SpirvConstants();
    
    SpirvCodeBuffer code() const;
    
  private:
    
    SpirvCodeBuffer m_code;
    
  };
  
}