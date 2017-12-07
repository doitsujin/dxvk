#pragma once

#include <spirv/spirv.hpp>
#include <spirv/GLSL.std.450.hpp>

#include "spirv_include.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V instruction
   * 
   * Helps parsing a single instruction, providing
   * access to the op code, instruction length and
   * instruction arguments.
   */
  class SpirvInstruction {
    
  public:
    
    SpirvInstruction() { }
    SpirvInstruction(
      const uint32_t* code, uint32_t size)
    : m_code(code), m_size(size) { }
    
    /**
     * \brief SPIR-V Op code
     * \returns The op code
     */
    spv::Op opCode() const {
      return static_cast<spv::Op>(
        this->arg(0) & spv::OpCodeMask);
    }
    
    /**
     * \brief Instruction length
     * \returns Number of DWORDs
     */
    uint32_t length() const {
      return this->arg(0) >> spv::WordCountShift;
    }
    
    /**
     * \brief Argument value
     * 
     * Retrieves an argument DWORD. Note that some instructions
     * take 64-bit arguments which require more than one DWORD.
     * Arguments start at index 1. Calling this method with an
     * argument ID of 0 will return the opcode token.
     * \param [in] id Argument index, starting at 1
     * \returns The argument value
     */
    uint32_t arg(uint32_t id) const {
      return id < m_size ? m_code[id] : 0;
    }
    
  private:
    
    uint32_t const* m_code = nullptr;
    uint32_t        m_size = 0;
    
  };
  
  
  /**
   * \brief SPIR-V instruction
   * 
   * Convenient iterator that can be used
   * to process raw SPIR-V shader code.
   */
  class SpirvInstructionIterator {
    
  public:
    
    SpirvInstructionIterator() { }
    SpirvInstructionIterator(const uint32_t* code, uint32_t size)
    : m_code(size != 0 ? code : nullptr), m_size(size) {
      if ((size >= 5) && (m_code[0] == spv::MagicNumber))
        this->advance(5);
    }
    
    SpirvInstructionIterator& operator ++ () {
      this->advance(SpirvInstruction(m_code, m_size).length());
      return *this;
    }
    
    SpirvInstruction operator * () const {
      return SpirvInstruction(m_code, m_size);
    }
    
    bool operator == (const SpirvInstructionIterator& other) const {
      return this->m_code == other.m_code
          && this->m_size == other.m_size;
    }
    
    bool operator != (const SpirvInstructionIterator& other) const {
      return this->m_code != other.m_code
          && this->m_size != other.m_size;
    }
    
  private:
    
    uint32_t const* m_code = nullptr;
    uint32_t        m_size = 0;
    
    void advance(uint32_t n) {
      if (n < m_size) {
        m_code += n;
        m_size -= n;
      } else {
        m_code = nullptr;
        m_size = 0;
      }
    }
    
  };
  
}