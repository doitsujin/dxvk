#include "spirv_gen_capability.h"

namespace dxvk {
  
  SpirvCapabilities:: SpirvCapabilities() { }
  SpirvCapabilities::~SpirvCapabilities() { }
  
  
  SpirvCodeBuffer SpirvCapabilities::code() const {
    return m_code;
  }
  
  
  void SpirvCapabilities::enable(spv::Capability cap) {
    // Scan the generated instructions to check
    // whether we already enabled the capability.
    for (auto ins : m_code) {
      if (ins.opCode() == spv::OpCapability && ins.arg(1) == cap)
        return;
    }
    
    m_code.putIns (spv::OpCapability, 2);
    m_code.putWord(cap);
  }
  
}