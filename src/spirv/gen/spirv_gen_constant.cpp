#include "spirv_gen_constant.h"

namespace dxvk {
  
  SpirvConstants:: SpirvConstants() { }
  SpirvConstants::~SpirvConstants() { }
  
  
  SpirvCodeBuffer SpirvConstants::code() const {
    return m_code;
  }
  
}