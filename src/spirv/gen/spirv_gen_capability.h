#pragma once

#include "../spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V capability set
   * 
   * Holds a code buffer solely for the \c OpCapability
   * instructions in the generated SPIR-V shader module.
   */
  class SpirvCapabilities {
    
  public:
    
    SpirvCapabilities();
    ~SpirvCapabilities();
    
    /**
     * \brief Code buffer
     * 
     * Code buffer that contains the
     * \c OpCapability instructions.
     * \returns Code buffer
     */
    SpirvCodeBuffer code() const;
    
    /**
     * \brief Enables a capability
     * 
     * If the given capability has not been explicitly
     * enabled yet, this will generate an \c OpCapability
     * instruction for the given capability.
     * \param [in] cap The capability
     */
    void enable(spv::Capability cap);
    
  private:
    
    SpirvCodeBuffer m_code;
    
  };
  
}