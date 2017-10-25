#include "dxbc_decoder.h"

namespace dxvk {
  
  DxbcCodeReader& DxbcCodeReader::operator ++ () {
    return this->operator += (1);
  }
  
  
  DxbcCodeReader& DxbcCodeReader::operator += (uint32_t n) {
    if (n < m_size) {
      m_code += n;
      m_size -= n;
    } else {
      m_code = nullptr;
      m_size = 0;
    }
    return *this;
  }
  
  
  DxbcCodeReader DxbcCodeReader::operator + (uint32_t n) const {
    return n < m_size
      ? DxbcCodeReader(m_code + n, m_size - n)
      : DxbcCodeReader();
  }
  
  
  bool DxbcCodeReader::operator == (const DxbcCodeReader& other) const {
    return m_code == other.m_code && m_size == other.m_size;
  }
  
  
  bool DxbcCodeReader::operator != (const DxbcCodeReader& other) const {
    return !this->operator == (other);
  }
  
  
  DxbcOperand::DxbcOperand(const DxbcCodeReader& code)
  : m_info(code) {
    DxbcOperandToken token(m_info.getWord(0));
    
    uint32_t numOperandTokens = 1;
    
    if (token.isExtended()) {
      while (DxbcOperandTokenExt(m_info.getWord(numOperandTokens++)).isExtended())
        continue;
    }
    
    m_data = m_info + numOperandTokens;
  }
  
  
  std::optional<DxbcOperandTokenExt> DxbcOperand::queryOperandExt(DxbcOperandExt ext) const {
    if (!this->token().isExtended())
      return { };
    
    uint32_t extTokenId = 1;
    DxbcOperandTokenExt extToken;
    
    do {
      extToken = m_info.getWord(extTokenId++);
      
      if (extToken.type() == ext)
        return extToken;
    } while (extToken.isExtended());
    
    return { };
  }
  
  
  uint32_t DxbcOperand::length() const {
    
  }
  
  
  DxbcInstruction::DxbcInstruction(const DxbcCodeReader& code)
  : m_op(code) {
    DxbcOpcodeToken token(m_op.getWord(0));
    
    if (token.opcode() == DxbcOpcode::CustomData) {
      // Custom data blocks have a special format,
      // the length is stored in a separate DWORD
      m_args = m_op + 2;
    } else {
      // For normal instructions, we just count
      // the number of extended opcode tokens.
      uint32_t numOpcodeTokens = 1;
      
      if (token.isExtended()) {
        while (DxbcOpcodeTokenExt(m_op.getWord(numOpcodeTokens++)).isExtended())
          continue;
      }
      
      m_args = m_op + numOpcodeTokens;
    }
  }
  
  
  uint32_t DxbcInstruction::length() const {
    auto token = this->token();
    return token.opcode() != DxbcOpcode::CustomData
      ? token.length() : m_op.getWord(1);
  }
  
  
  std::optional<DxbcOpcodeTokenExt> DxbcInstruction::queryOpcodeExt(DxbcExtOpcode extOpcode) const {
    if (!this->token().isExtended())
      return { };
    
    uint32_t extTokenId = 1;
    DxbcOpcodeTokenExt extToken;
    
    do {
      extToken = m_op.getWord(extTokenId++);
      
      if (extToken.opcode() == extOpcode)
        return extToken;
    } while (extToken.isExtended());
    
    return { };
  }
  
}