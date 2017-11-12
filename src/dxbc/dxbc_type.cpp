#include "dxbc_type.h"

namespace dxvk {
  
  DxbcComponentSwizzle DxbcComponentSwizzle::extract(DxbcComponentMask mask) const {
    DxbcComponentSwizzle result;
    
    uint32_t j = 0;
    for (uint32_t i = 0; i < m_components.size(); i++) {
      if (mask.test(i))
        result[j++] = m_components.at(i);
    }
    
    return result;
  }
  
}