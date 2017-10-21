#pragma once

#include "../spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V debug info generator
   * 
   * Can be used to assign names to result IDs,
   * such as variables, for debugging purposes.
   */
  class SpirvDebugInfo {
    
  public:
    
    SpirvDebugInfo();
    ~SpirvDebugInfo();
    
    const SpirvCodeBuffer& code() const {
      return m_code;
    }
    
    void assignName(
            uint32_t  id,
      const char*     name);
    
  private:
    
    SpirvCodeBuffer m_code;
    
  };
  
}