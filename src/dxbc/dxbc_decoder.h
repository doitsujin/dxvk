#pragma once

#include <cstring>

#include "dxbc_enums.h"

namespace dxvk {
  
  /**
   * \brief DXBC instruction token
   * 
   * Initial token at the beginning of each instruction.
   * This encodes the actual op code, the length of the
   * entire instruction in DWORDs, as well as some flags
   * controlling the specific instruction.
   */
  class DxbcOpcodeToken {
    
  public:
    
    DxbcOpcodeToken() { }
    DxbcOpcodeToken(uint32_t token)
    : m_token(token) { }
    
    /**
     * \brief Opcode
     * \returns Opcode
     */
    DxbcOpcode opcode() const {
      return static_cast<DxbcOpcode>(
        bit::extract(m_token, 0, 10));
    }
    
    /**
     * \brief Control info
     * 
     * Instruction-specific control info. Undefined
     * if the opcode is \c DxbcOpcode::CustomData.
     * \returns Control info
     */
    uint32_t control() const {
      return bit::extract(m_token, 11, 23);
    }
    
    /**
     * \brief Instruction length
     * 
     * Undefind if the opcode is \c DxbcOpcode::CustomData.
     * In that case, the instruction length will be stored
     * in the DWORD immediately following the opcode token.
     * \returns Instruction length, in DWORDs
     */
    uint32_t length() const {
      return bit::extract(m_token, 24, 30);
    }
    
    /**
     * \brief Checks whether the opcode is extended
     * 
     * Additional information is encoded in extended
     * opcode tokens if this flag is set. Note that
     * multiple extended opcode tokens may be chained.
     * \returns \c true if the opcode is extended.
     */
    bool isExtended() const {
      return !!bit::extract(m_token, 31, 31);
    }
    
  private:
    
    uint32_t m_token = 0;
    
  };
  
  
  /**
   * \brief DXBC extended instruction token
   * 
   * Some instruction may encode additional control
   * modifiers in extended opcode tokens. The format
   * of these tokens differs from that of the the
   * initial opcode tokens.
   */
  class DxbcOpcodeTokenExt {
    
  public:
    
    DxbcOpcodeTokenExt() { }
    DxbcOpcodeTokenExt(uint32_t token)
    : m_token(token) { }
    
    /**
     * \brief Control info
     * 
     * Instruction-specific control info. Undefined
     * if the opcode is \c DxbcOpcode::CustomData.
     * \returns Control info
     */
    uint32_t control() const {
      return bit::extract(m_token, 6, 30);
    }
    
    /**
     * \brief Extended opcode length
     * 
     * Number of DWORDs that belong to this extended
     * opcode information. Currently, there are no
     * extended opcodes with a length greater than 1.
     * \returns Exteded opcode length, in DWORDs
     */
    uint32_t length() const {
      return 1;
    }
    
    /**
     * \brief Checks whether there are additional modifiers
     * \returns \c true if the extended opcode is extended
     */
    bool isExtended() const {
      return !!bit::extract(m_token, 31, 31);
    }
    
  private:
    
    uint32_t m_token = 0;
    
  };
  
  
  /**
   * \brief Operand token
   * 
   * Stores general information about one operand of an
   * instruction. Like opcode tokens, operand tokens may
   * be extended.
   */
  class DxbcOperandToken {
    
  public:
    
    DxbcOperandToken(uint32_t token)
    : m_token(token) { }
    
    /**
     * \brief Number of operand components
     * 
     * The number of components that the operand
     * has. Can be zero, one, or four.
     * \returns Number of components
     */
    DxbcOperandNumComponents numComponents() const {
      return static_cast<DxbcOperandNumComponents>(
        bit::extract(m_token, 0, 1));
    }
    
    /**
     * \brief Component selection mode
     * 
     * Operands can be either masked so that some components
     * will not be used, or they can be swizzled so that only
     * a given set of components is used.
     * \returns Component selection mode
     */
    DxbcOperandComponentSelectionMode selectionMode() const {
      return static_cast<DxbcOperandComponentSelectionMode>(
        bit::extract(m_token, 2, 3));
    }
    
    /**
     * \brief Operand type
     * 
     * Selects the type of the operand, i.e. whether the
     * operand is a temporary register, a shader resource
     * or a builtin interface variable.
     * \returns Operand type
     */
    DxbcOperandType type() const {
      return static_cast<DxbcOperandType>(
        bit::extract(m_token, 12, 19));
    }
    
    /**
     * \brief Index dimension
     * 
     * Number of indices. In DXBC, each register file has
     * a dimensionality, e.g. the temporary registers are
     * one-dimensional and therefore require one index.
     * \returns Number of index dimensions
     */
    uint32_t indexDimension() const {
      return bit::extract(m_token, 20, 21);
    }
    
    /**
     * \brief Index representation
     * 
     * Stores whether an index is stored as an
     * immediate value or as a relative value
     * which requires another operand token.
     * \param [in] dim Index dimension to query
     * \returns Representation of that index
     */
    DxbcOperandIndexRepresentation indexRepresentation(uint32_t dim) const {
      return static_cast<DxbcOperandIndexRepresentation>(
        bit::extract(m_token, 22 + 3 * dim, 24 + 3 * dim));
    }
    
    /**
     * \brief Checks whether the operand is extended
     * 
     * Operands can be modified with extended tokens.
     * \returns \c true if the operand is extended
     */
    bool isExtended() const {
      return !!bit::extract(m_token, 31, 31);
    }
    
  private:
    
    uint32_t m_token = 0;
    
  };
  
  
  /**
   * \brief Extended operand token
   * 
   * Stores additional properties for an operand that
   * cannot be stored in the initial operand token.
   */
  class DxbcOperandTokenExt {
    
  public:
    
    DxbcOperandTokenExt() { }
    DxbcOperandTokenExt(uint32_t token)
    : m_token(token) { }
    
    /**
     * \brief Operand extension type
     * \returns Operand extension type
     */
    DxbcOperandExt type() const {
      return static_cast<DxbcOperandExt>(
        bit::extract(m_token, 0, 5));
    }
    
    /**
     * \brief Data flags
     * \returns Data flags
     */
    uint32_t data() const {
      return bit::extract(m_token, 6, 30);
    }
    
    /**
     * \brief Checks whether the operand is extended
     * 
     * Like extended opcode tokens, extended
     * operand tokens can be chained.
     * \returns \c true if extended
     */
    bool isExtended() const {
      return !!bit::extract(m_token, 31, 31);
    }
    
  private:
    
    uint32_t m_token = 0;
    
  };
  
  
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
    DxbcInstruction(
      const uint32_t* code,
            uint32_t  size);
    
    /**
     * \brief Retrieves instruction word
     * 
     * \param [in] Instruction word ID
     * \returns The instruction word
     */
    uint32_t getWord(uint32_t id) const;
    
    /**
     * \brief Instruction length
     * 
     * Number of DWORDs for this instruction,
     * including the initial opcode token.
     * \returns Instruction length
     */
    uint32_t length() const;
    
  private:
    
    const uint32_t* m_code = nullptr;
          uint32_t  m_size = 0;
    
  };
  
  
  /**
   * \brief Instruction decoder
   * 
   * Helper class that provides methods to read typed
   * tokens and immediate values from the instruction
   * stream. This will read instructions word by word.
   */
  class DxbcInstructionDecoder {
    
  public:
    
    DxbcInstructionDecoder() { }
    DxbcInstructionDecoder(
      const DxbcInstruction& inst)
    : m_inst(inst) { }
    
    /**
     * \brief Reads opcode token
     * 
     * Must be the very first call.
     * \returns The opcode token
     */
    DxbcOpcodeToken readOpcode() {
      return DxbcOpcodeToken(m_inst.getWord(m_word++));
    }
    
    /**
     * \brief Reads extended opcode token
     * \returns Extended opcode token
     */
    DxbcOpcodeTokenExt readOpcodeExt() {
      return DxbcOpcodeTokenExt(m_inst.getWord(m_word++));
    }
    
    /**
     * \brief Reads operand token
     * \returns Next operand token
     */
    DxbcOperandToken readOperand() {
      return DxbcOperandToken(m_inst.getWord(m_word++));
    }
    
    /**
     * \brief Reads extended operand token
     * \returns Extended operand token
     */
    DxbcOperandTokenExt readOperandExt() {
      return DxbcOperandTokenExt(m_inst.getWord(m_word++));
    }
    
    /**
     * \brief Reads immediate 32-bit integer
     * \returns The 32-bit integer constant
     */
    uint32_t readu32() {
      return m_inst.getWord(m_word++);
    }
    
    /**
     * \brief Reads immediate 64-bit integer
     * \returns The 64-bit integer constant
     */
    uint64_t readu64() {
      uint64_t hi = readu32();
      uint64_t lo = readu32();
      return (hi << 32) | lo;
    }
    
    /**
     * \brief Reads immediate 32-bit float
     * \returns The 32-bit float constant
     */
    float readf32() {
      float result;
      uint32_t integer = readu32();
      std::memcpy(&result, &integer, sizeof(float));
      return result;
    }
    
    /**
     * \brief Reads immediate 64-bit float
     * \returns The 64-bit float constant
     */
    double readf64() {
      double result;
      uint64_t integer = readu64();
      std::memcpy(&result, &integer, sizeof(double));
      return result;
    }
    
  private:
    
    DxbcInstruction m_inst = { nullptr, 0u };
    uint32_t        m_word = 0;
    
  };
  
  
  /**
   * \brief DXBC instruction decoder
   * 
   * Iterator that walks over DXBC instructions.
   * Instruction boundaries are easy to find as
   * the length of each instruction is encoded
   * in the opcode token, much like in SPIR-V.
   */
  class DxbcDecoder {
    
  public:
    
    DxbcDecoder() { }
    DxbcDecoder(const uint32_t* code, uint32_t size);
    
    DxbcDecoder& operator ++ ();
    
    DxbcInstruction operator * () const;
    
    bool operator == (const DxbcDecoder& other) const { return m_code == other.m_code; }
    bool operator != (const DxbcDecoder& other) const { return m_code != other.m_code; }
    
  private:
    
    const uint32_t* m_code = nullptr;
          uint32_t  m_size = 0;
    
  };
  
}