#pragma once

#include <cstring>
#include <optional>

#include "dxbc_enums.h"
#include "dxbc_names.h"
#include "dxbc_type.h"

namespace dxvk {
  
  class DxbcOperand;
  
  /**
   * \brief Basic control info
   * 
   * Parses instruction-specific control bits. Whether
   * these are well defined depends on the instruction.
   */
  class DxbcOpcodeControl {
    
  public:
    
    DxbcOpcodeControl() { }
    DxbcOpcodeControl(uint32_t control)
    : m_control(control) { }
    
    /**
     * \brief Saturation hint
     * 
     * If set, the result of the given instruction
     * is clamped to the [0..1] range.
     */
    bool saturateBit() const {
      return bit::extract(m_control, 2, 2) != 0;
    }
    
    /**
     * \brief Precision hint
     */
    bool preciseBit() const {
      return bit::extract(m_control, 8, 11) != 0;
    }
    
    /**
     * \brief Zero test
     * 
     * For conditional instructions, this defines
     * whether the test shall pass when the given
     * operand is zero or non-zero.
     */
    DxbcZeroTest zeroTest() const {
      return static_cast<DxbcZeroTest>(
        bit::extract(m_control, 7, 7));
    }
    
    /**
     * \brief Resinfo return type
     * 
     * Control bits specifically for
     * the \c resinfo instruction.
     */
    DxbcResinfoType resinfoType() const {
      return static_cast<DxbcResinfoType>(
        bit::extract(m_control, 0, 1));
    }
    
    /**
     * \brief Sync flags
     * 
     * Defines the exact operation of sync
     * instructions in compute shaders.
     */
    DxbcSyncFlags syncFlags() const {
      return bit::extract(m_control, 0, 3);
    }
    
  private:
    
    uint32_t m_control = 0;
    
  };
  
  
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
     * \brief Extended opcode
     * \returns Extended opcode
     */
    DxbcExtOpcode opcode() const {
      return static_cast<DxbcExtOpcode>(
        bit::extract(m_token, 0, 5));
    }
    
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
    uint32_t numComponents() const {
      std::array<uint32_t, 3> count = { 0, 1, 4 };
      return count.at(bit::extract(m_token, 0, 1));
    }
    
    /**
     * \brief Component selection mode
     * 
     * Operands can be either masked so that some components
     * will not be used, or they can be swizzled so that only
     * a given set of components is used.
     * \returns Component selection mode
     */
    DxbcComponentSelectionMode selectionMode() const {
      return static_cast<DxbcComponentSelectionMode>(
        bit::extract(m_token, 2, 3));
    }
    
    /**
     * \brief Component mask
     * 
     * Used when the component selection mode is
     * \c DxbcComponentSelectionMode::Mask.
     * \returns The component mask
     */
    DxbcComponentMask componentMask() const {
      return DxbcComponentMask(bit::extract(m_token, 4, 7));
    }
    
    /**
     * \brief Component swizzle
     * 
     * Used when the component selection mode is
     * \c DxbcComponentSelectionMode::Swizzle.
     * \returns The component swizzle
     */
    DxbcComponentSwizzle componentSwizzle() const {
      return DxbcComponentSwizzle(
        bit::extract(m_token,  4,  5),
        bit::extract(m_token,  6,  7),
        bit::extract(m_token,  8,  9),
        bit::extract(m_token, 10, 11));
    }
    
    /**
     * \brief Component selection
     * 
     * Used when the component selection mode is
     * \c DxbcComponentSelectionMode::Select1.
     */
    DxbcComponentMask componentSelection() const {
      return DxbcComponentMask(bit::extract(m_token, 4, 5));
    }
    
    /**
     * \brief Interpolatin mode
     * 
     * Used by input declarations in pixel shaders.
     * Undefined for all other instructions.
     */
    DxbcInterpolationMode interpolationMode() const {
      return static_cast<DxbcInterpolationMode>(
        bit::extract(m_token, 11, 14));
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
   * \brief DXBC code DxbcCodeReader
   * 
   * Helper class that can read DWORDs from a sized slice.
   * Returns undefined numbers on out-of-bounds access, but
   * makes sure not to access memory locations outside the
   * original code array.
   */
  class DxbcCodeReader {
    
  public:
    
    DxbcCodeReader() { }
    DxbcCodeReader(
      const uint32_t* code,
            uint32_t  size)
    : m_code(size != 0 ? code : nullptr),
      m_size(size) { }
    
    uint32_t getWord(uint32_t id) const {
      return id < m_size ? m_code[id] : 0;
    }
    
    DxbcCodeReader& operator ++ ();
    DxbcCodeReader& operator += (uint32_t n);
    DxbcCodeReader operator + (uint32_t n) const;
    
    bool operator == (const DxbcCodeReader& other) const;
    bool operator != (const DxbcCodeReader& other) const;
    
  private:
    
    const uint32_t* m_code = nullptr;
          uint32_t  m_size = 0;
    
  };
  
  
  /**
   * \brief DXBC operand index
   * 
   * Represents an index into an indexable register file.
   * For each register file dimension, one operand index
   * must be read from the encoded instruction.
   */
  class DxbcOperandIndex {
    
  public:
    
    DxbcOperandIndex() { }
    DxbcOperandIndex(
      const DxbcCodeReader&                 code,
            DxbcOperandIndexRepresentation  rep)
    : m_code(code), m_rep(rep) { }
    
    uint32_t length() const;
    
    bool hasImmPart() const;
    bool hasRelPart() const;
    
    uint64_t    immPart() const;
    DxbcOperand relPart() const;
    
  private:
    
    DxbcCodeReader                  m_code;
    DxbcOperandIndexRepresentation  m_rep;
    
  };
  
  
  /**
   * \brief DXBC operand
   * 
   * Provides methods to query the operand token
   * including extended operand tokens, which may
   * modify the operand's return value.
   */
  class DxbcOperand {
    
  public:
    
    DxbcOperand() { }
    DxbcOperand(const DxbcCodeReader& code);
    
    /**
     * \brief Operand token
     * \returns Operand token
     */
    DxbcOperandToken token() const {
      return DxbcOperandToken(m_info.getWord(0));
    }
    
    /**
     * \brief Operand length, in DWORDs
     * \returns Number of DWORDs
     */
    uint32_t length() const {
      return m_length;
    }
    
    /**
     * \brief Operand index for a single dimension
     * 
     * \param [in] dim Dimension to query
     * \returns Operand index
     */
    DxbcOperandIndex index(uint32_t dim) const;
    
    /**
     * \brief Queries an operand extension
     * 
     * If an extended operand token with the given
     * operand extension exists, return that token.
     * \param [in] ext The operand extension
     * \returns The extended operand token
     */
    std::optional<DxbcOperandTokenExt> queryOperandExt(
            DxbcOperandExt ext) const;
    
    /**
     * \brief Reads 32-bit immediate integer
     * 
     * \param [in] idx Component index
     * \returns The immediate operand
     */
    uint32_t imm32(uint32_t idx) const {
      return m_data.getWord(idx);
    }
    
    /**
     * \brief Reads 64-bit immediate integer
     * 
     * \param [in] idx Component index
     * \returns The immediate operand
     */
    uint64_t imm64(uint32_t idx) const {
      uint64_t hi = m_data.getWord(2 * idx + 0);
      uint64_t lo = m_data.getWord(2 * idx + 1);
      return (hi << 32) | (lo);
    }
    
  private:
    
    DxbcCodeReader m_info;
    DxbcCodeReader m_data;
    
    uint32_t                m_length       = 0;
    std::array<uint32_t, 3> m_indexOffsets = { 0, 0, 0 };
    
  };
  
  
  /**
   * \brief DXBC instruction
   * 
   * Provides methods to query the opcode token
   * including extended opcode tokens, as well
   * as convenience methods to read operands.
   */
  class DxbcInstruction {
    
  public:
    
    DxbcInstruction() { }
    DxbcInstruction(const DxbcCodeReader& code);
    
    /**
     * \brief Opcode token
     * \returns Opcode token
     */
    DxbcOpcodeToken token() const {
      return DxbcOpcodeToken(m_op.getWord(0));
    }
    
    /**
     * \brief Instruction length, in DWORDs
     * \returns Instruction length, in DWORDs
     */
    uint32_t length() const;
    
    /**
     * \brief Queries an opcode extension
     * 
     * If an extended opcode token with the given
     * opcode exists, the token will be returned.
     * \param extOpcode Extended opcode
     * \returns Extended opcode token
     */
    std::optional<DxbcOpcodeTokenExt> queryOpcodeExt(
            DxbcExtOpcode extOpcode) const;
    
    /**
     * \brief Retrieves argument word
     * 
     * Instruction arguments immediately follow the opcode
     * tokens, including the extended opcodes. Argument 0
     * will therefore be the first DWORD that is part of
     * an instruction operand or an immediate number.
     * \param [in] idx Argument word index
     * \returns The word at the given index
     */
    uint32_t arg(uint32_t idx) const {
      return m_args.getWord(idx);
    }
    
    /**
     * \brief Reads an argument enum
     * 
     * Casts the word at the given location to an enum.
     * Some instructions take name tokens as operands.
     * \param [in] idx Argument word index
     * \returns The enum value of the given word
     */
    template<typename T>
    T readEnum(uint32_t idx) const {
      return static_cast<T>(arg(idx));
    }
    
    /**
     * \brief Retrieves an operand
     * 
     * \param [in] idx Argument word index
     * \returns The operand object
     */
    DxbcOperand operand(uint32_t idx) const {
      return DxbcOperand(m_args + idx);
    }
    
  private:
    
    DxbcCodeReader m_op;
    DxbcCodeReader m_args;
    
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
    DxbcDecoder(const uint32_t* code, uint32_t size)
    : m_code(code, size) { }
    
    DxbcDecoder& operator ++ () {
      m_code += DxbcInstruction(m_code).length();
      return *this;
    }
    
    DxbcInstruction operator * () const {
      return DxbcInstruction(m_code);
    }
    
    bool operator == (const DxbcDecoder& other) const { return m_code == other.m_code; }
    bool operator != (const DxbcDecoder& other) const { return m_code != other.m_code; }
    
  private:
    
    DxbcCodeReader m_code;
    
  };
  
}