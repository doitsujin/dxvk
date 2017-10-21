#include "spirv_gen_debuginfo.h"

namespace dxvk {
  
  SpirvDebugInfo:: SpirvDebugInfo() { }
  SpirvDebugInfo::~SpirvDebugInfo() { }
  
  
  void SpirvDebugInfo::assignName(
          uint32_t  id,
    const char*     name) {
    m_code.putIns (spv::OpName, 2 + m_code.strLen(name));
    m_code.putWord(id);
    m_code.putStr (name);
  }
  
}