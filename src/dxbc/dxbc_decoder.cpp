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
  
  
  uint32_t DxbcOperandIndex::length() const {
    switch (m_rep) {
      case DxbcOperandIndexRepresentation::Imm32:         return 1;
      case DxbcOperandIndexRepresentation::Imm64:         return 2;
      case DxbcOperandIndexRepresentation::Relative:      return this->relPart().length();
      case DxbcOperandIndexRepresentation::Imm32Relative: return this->relPart().length() + 1;
      case DxbcOperandIndexRepresentation::Imm64Relative: return this->relPart().length() + 2;
    }
    
    throw DxvkError(str::format("DXBC: Unknown index representation: ", m_rep));
  }
  
  
  bool DxbcOperandIndex::hasImmPart() const {
    return m_rep == DxbcOperandIndexRepresentation::Imm32
        || m_rep == DxbcOperandIndexRepresentation::Imm64
        || m_rep == DxbcOperandIndexRepresentation::Imm32Relative
        || m_rep == DxbcOperandIndexRepresentation::Imm64Relative;
  }
  
  
  bool DxbcOperandIndex::hasRelPart() const {
    return m_rep == DxbcOperandIndexRepresentation::Relative
        || m_rep == DxbcOperandIndexRepresentation::Imm32Relative
        || m_rep == DxbcOperandIndexRepresentation::Imm64Relative;
  }
  
  
  uint64_t DxbcOperandIndex::immPart() const {
    switch (m_rep) {
      case DxbcOperandIndexRepresentation::Imm32:
      case DxbcOperandIndexRepresentation::Imm32Relative:
        return m_code.getWord(0);
      
      case DxbcOperandIndexRepresentation::Imm64:
      case DxbcOperandIndexRepresentation::Imm64Relative:
        return (static_cast<uint64_t>(m_code.getWord(0)) << 32)
             | (static_cast<uint64_t>(m_code.getWord(1)));
      
      default:
        return 0;
    }
  }
  
  
  DxbcOperand DxbcOperandIndex::relPart() const {
    switch (m_rep) {
      case DxbcOperandIndexRepresentation::Relative:
        return DxbcOperand(m_code);
        
      case DxbcOperandIndexRepresentation::Imm32Relative:
        return DxbcOperand(m_code + 1);
        
      case DxbcOperandIndexRepresentation::Imm64Relative:
        return DxbcOperand(m_code + 2);
        
      default:
        throw DxvkError("DXBC: Operand index is not relative");
    }
  }
  
  
  DxbcOperand::DxbcOperand(const DxbcCodeReader& code)
  : m_info(code) {
    const DxbcOperandToken token(m_info.getWord(0));
    
    uint32_t numTokens = 1;
    
    // Count extended operand tokens
    if (token.isExtended()) {
      while (DxbcOperandTokenExt(m_info.getWord(numTokens++)).isExtended())
        continue;
    }
    
    m_data = m_info + numTokens;
    
    // Immediate operands
    uint32_t length = 0;
    
    if (token.type() == DxbcOperandType::Imm32
     || token.type() == DxbcOperandType::Imm64)
      length += token.numComponents();
    
    // Indices into the register file, may contain additional operands
    for (uint32_t i = 0; i < token.indexDimension(); i++) {
      m_indexOffsets[i] = length;
      length += this->index(i).length();
    }
    
    m_length = length + numTokens;
  }
  
  
  DxbcOperandIndex DxbcOperand::index(uint32_t dim) const {
    return DxbcOperandIndex(
      m_data + m_indexOffsets.at(dim),
      this->token().indexRepresentation(dim));
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
        numOpcodeTokens += 1;
        while (DxbcOpcodeTokenExt(m_op.getWord(numOpcodeTokens)).isExtended())
          numOpcodeTokens += 1;
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