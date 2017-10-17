#include "dxbc_capability.h"

namespace dxvk {
  
  DxbcCapabilities:: DxbcCapabilities() { }
  DxbcCapabilities::~DxbcCapabilities() { }
  
  
  DxvkSpirvCodeBuffer DxbcCapabilities::code() const {
    DxvkSpirvCodeBuffer code;
    
    for (auto cap : m_caps) {
      code.putIns (spv::OpCapability, 2);
      code.putWord(cap);
    }
    
    return code;
  }
  
  
  void DxbcCapabilities::enable(spv::Capability cap) {
    m_caps.insert(cap);
  }
  
}