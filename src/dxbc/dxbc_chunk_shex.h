#pragma once

#include "dxbc_common.h"
#include "dxbc_instruction.h"
#include "dxbc_reader.h"

namespace dxvk {
  
  /**
   * \brief Shader code chunk
   * 
   * Stores the DXBC shader code itself, as well
   * as some meta info about the shader, i.e. what
   * type of shader this is.
   */
  class DxbcShex : public RcObject {
    
  public:
    
    DxbcShex(DxbcReader reader);
    ~DxbcShex();
    
    DxbcProgramVersion version() const {
      return m_version;
    }
    
    DxbcInstructionIterator begin() const {
      return DxbcInstructionIterator(m_code.data());
    }
    
    DxbcInstructionIterator end() const {
      return DxbcInstructionIterator(m_code.data() + m_code.size());
    }
    
  private:
    
    DxbcProgramVersion    m_version;
    std::vector<uint32_t> m_code;
    
  };
  
}