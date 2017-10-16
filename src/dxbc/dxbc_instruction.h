#pragma once

#include "dxbc_opcode.h"

namespace dxvk {
  
  /**
   * \brief DXBC instruction
   * 
   * Provides convenience methods to extract the
   * opcode, instruction length, and instruction
   * arguments from an instruction.
   */
  class DxbcInstruction {
    
  public:
    
    DxbcInstruction() { }
    DxbcInstruction(const uint32_t* code)
    : m_code(code) { }
    
    /**
     * \brief Instruction code
     * \returns The operation code
     */
    DxbcOpcode opcode() const {
      return static_cast<DxbcOpcode>(
        bit::extract<uint32_t, 0, 10>(m_code[0]));
    }
    
    /**
     * \brief Instruction length
     * 
     * Number of DWORDs for this instruction,
     * including the initial opcode token.
     * \returns Instruction length
     */
    uint32_t length() const {
      return this->opcode() != DxbcOpcode::CustomData
        ? bit::extract<uint32_t, 24, 30>(m_code[0])
        : m_code[1];
    }
    
  private:
    
    const uint32_t* m_code = nullptr;
    
  };
  
  
  /**
   * \brief DXBC instruction iterator
   * 
   * Iterator that walks over DXBC instructions.
   * Instruction boundaries are easy to find as
   * the length of each instruction is encoded
   * in the opcode token, much like in SPIR-V.
   */
  class DxbcInstructionIterator {
    
  public:
    
    DxbcInstructionIterator() { }
    DxbcInstructionIterator(const uint32_t* code)
    : m_code(code) { }
    
    DxbcInstructionIterator& operator ++ () {
      m_code += DxbcInstruction(m_code).length();
      return *this;
    }
    
    DxbcInstruction operator * () const {
      return DxbcInstruction(m_code);
    }
    
    bool operator == (const DxbcInstructionIterator& other) const { return m_code == other.m_code; }
    bool operator != (const DxbcInstructionIterator& other) const { return m_code != other.m_code; }
    
  private:
    
    const uint32_t* m_code = nullptr;
    
  };
  
}