#include "dxbc_decoder.h"

namespace dxvk {
  
  DxbcInstruction::DxbcInstruction(
    const uint32_t* code,
          uint32_t  size)
  : m_code(code), m_size(size) {
    
  }
  
  
  uint32_t DxbcInstruction::getWord(uint32_t id) const {
    return id < m_size ? m_code[id] : 0;
  }
  
  
  uint32_t DxbcInstruction::length() const {
    const DxbcOpcodeToken token(getWord(0));
    return token.opcode() != DxbcOpcode::CustomData
      ? token.length()
      : getWord(1);
  }
    
    
  DxbcDecoder::DxbcDecoder(const uint32_t* code, uint32_t size)
  : m_code(size != 0 ? code : nullptr), m_size(size) { }
  
  
  DxbcDecoder& DxbcDecoder::operator ++ () {
    auto len = DxbcInstruction(m_code, m_size).length();
    if (len < m_size) {
      m_code += len;
      m_size -= len;
    } else {
      m_code = nullptr;
      m_size = 0;
    }
    return *this;
  }
  
  
  DxbcInstruction DxbcDecoder::operator * () const {
    return DxbcInstruction(m_code, m_size);
  }
  
}