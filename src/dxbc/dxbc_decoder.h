#pragma once

#include <array>

#include "dxbc_common.h"
#include "dxbc_decoder.h"
#include "dxbc_defs.h"
#include "dxbc_enums.h"
#include "dxbc_names.h"

namespace dxvk {
  
  constexpr size_t DxbcMaxRegIndexDim = 3;
  
  struct DxbcRegister;
  
  /**
   * \brief Source operand modifiers
   * 
   * These are applied after loading
   * an operand register.
   */
  enum class DxbcRegModifier : uint32_t {
    Neg = 0,
    Abs = 1,
  };
  
  using DxbcRegModifiers = Flags<DxbcRegModifier>;
  
  
  /**
   * \brief Constant buffer binding
   * 
   * Stores information required to
   * access a constant buffer.
   */
  struct DxbcConstantBuffer {
    uint32_t varId = 0;
    uint32_t size  = 0;
  };
  
  /**
   * \brief Sampler binding
   * 
   * Stores a sampler variable that can be
   * used together with a texture resource.
   */
  struct DxbcSampler {
    uint32_t varId  = 0;
    uint32_t typeId = 0;
  };
  
  
  /**
   * \brief Shader resource binding
   * 
   * Stores a resource variable
   * and associated type IDs.
   */
  struct DxbcShaderResource {
    uint32_t varId         = 0;
    uint32_t sampledTypeId = 0;
    uint32_t textureTypeId = 0;
  };
  
  /**
   * \brief Component swizzle
   * 
   * Maps vector components to
   * other vector components.
   */
  class DxbcRegSwizzle {
    
  public:
    
    DxbcRegSwizzle() { }
    DxbcRegSwizzle(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
    : m_data((x << 0) | (y << 2) | (z << 4) | (w << 6)) { }
    
    uint32_t operator [] (uint32_t id) const {
      return (m_data >> (id + id)) & 0x3;
    }
    
  private:
    
    uint8_t m_data = 0;
    
  };
  
  
  /**
   * \brief Component mask
   * 
   * Enables access to certain
   * subset of vector components.
   */
  class DxbcRegMask {
    
  public:
    
    DxbcRegMask() { }
    DxbcRegMask(uint32_t mask) : m_data(mask) { }
    DxbcRegMask(bool x, bool y, bool z, bool w)
    : m_data((x ? 0x1 : 0) | (y ? 0x2 : 0)
           | (z ? 0x4 : 0) | (w ? 0x8 : 0)) { }
    
    bool operator [] (uint32_t id) const {
      return (m_data >> id) & 1;
    }
    
    uint32_t setCount() const {
      const uint8_t n[16] = { 0, 1, 1, 2, 1, 2, 2, 3,
                              1, 2, 2, 3, 2, 3, 3, 4 };
      return n[m_data & 0xF];
    }
    
    uint32_t firstSet() const {
      const uint8_t n[16] = { 4, 0, 1, 0, 2, 0, 1, 0,
                              3, 0, 1, 0, 2, 0, 1, 0 };
      return n[m_data & 0xF];
    }
    
  private:
    
    uint8_t m_data = 0;
    
  };
  
  
  /**
   * \brief System value mapping
   * 
   * Maps a system value to a given set of
   * components of an input or output register.
   */
  struct DxbcSvMapping {
    uint32_t        regId;
    DxbcRegMask     regMask;
    DxbcSystemValue sv;
  };
  
  
  struct DxbcRegIndex {
    DxbcRegister*     relReg;
    int32_t           offset;
  };
  
  
  /**
   * \brief Instruction operand
   * 
   * 
   */
  struct DxbcRegister {
    DxbcOperandType       type;
    DxbcScalarType        dataType;
    DxbcComponentCount    componentCount;
    
    uint32_t              idxDim;
    DxbcRegIndex          idx[DxbcMaxRegIndexDim];
    
    DxbcRegMask           mask;
    DxbcRegSwizzle        swizzle;
    DxbcRegModifiers      modifiers;
    
    union {
      uint32_t            u32_4[4];
      uint32_t            u32_1;
    } imm;
  };
  
  
  /**
   * \brief Instruction result modifiers
   * 
   * Modifiers that are applied
   * to all destination operands.
   */
  struct DxbcOpModifiers {
    bool saturate;
    bool precise;
  };
  
  
  /**
   * \brief Opcode controls
   * 
   * Instruction-specific controls. Usually,
   * only one of the members will be valid.
   */
  struct DxbcShaderOpcodeControls {
    DxbcZeroTest          zeroTest;
    DxbcSyncFlags         syncFlags;
    DxbcResourceDim       resourceDim;
    DxbcResinfoType       resinfoType;
    DxbcInterpolationMode interpolation;
  };
  
  
  /**
   * \brief Sample controls
   * 
   * Constant texel offset with
   * values raning from -8 to 7.
   */
  struct DxbcShaderSampleControls {
    int u, v, w;
  };
  
  
  /**
   * \brief Immediate value
   * 
   * Immediate argument represented either
   * as a 32-bit or 64-bit unsigned integer.
   */
  union DxbcImmediate {
    uint32_t u32;
    uint64_t u64;
  };
  
  
  /**
   * \brief Shader instruction
   */
  struct DxbcShaderInstruction {
    DxbcOpcode                op;
    DxbcInstClass             opClass;
    DxbcOpModifiers           modifiers;
    DxbcShaderOpcodeControls  controls;
    DxbcShaderSampleControls  sampleControls;
    
    uint32_t                  dstCount;
    uint32_t                  srcCount;
    uint32_t                  immCount;
    
    const DxbcRegister*       dst;
    const DxbcRegister*       src;
    const DxbcImmediate*      imm;
  };
  
  
  /**
   * \brief DXBC code slice
   * 
   * Convenient pointer pair that allows
   * reading the code word stream safely.
   */
  class DxbcCodeSlice {
    
  public:
    
    DxbcCodeSlice(
      const uint32_t* ptr,
      const uint32_t* end)
    : m_ptr(ptr), m_end(end) { }
    
    uint32_t at(uint32_t id) const;
    uint32_t read();
    
    DxbcCodeSlice take(uint32_t n) const;
    DxbcCodeSlice skip(uint32_t n) const;
    
    bool atEnd() const {
      return m_ptr == m_end;
    }
    
  private:
    
    const uint32_t* m_ptr = nullptr;
    const uint32_t* m_end = nullptr;
    
  };
  
  
  /**
   * \brief Decode context
   * 
   * Stores data that is required to decode a single
   * instruction. This data is not persistent, so it
   * should be forwarded to the compiler right away.
   */
  class DxbcDecodeContext {
    
  public:
    
    /**
     * \brief Retrieves current instruction
     * 
     * This is only valid after a call to \ref decode.
     * \returns Reference to last decoded instruction
     */
    const DxbcShaderInstruction& getInstruction() const {
      return m_instruction;
    }
    
    /**
     * \brief Decodes an instruction
     * 
     * This also advances the given code slice by the
     * number of dwords consumed by the instruction.
     * \param [in] code Code slice
     */
    void decodeInstruction(DxbcCodeSlice& code);
    
  private:
    
    DxbcShaderInstruction m_instruction;
    
    std::array<DxbcRegister,  4> m_dstOperands;
    std::array<DxbcRegister,  4> m_srcOperands;
    std::array<DxbcImmediate, 4> m_immOperands;
    std::array<DxbcRegister, 12> m_indices;
    
    // Index into the indices array. Used when decoding
    // instruction operands with relative indexing.
    uint32_t m_indexId = 0;
    
    void decodeCustomData(DxbcCodeSlice code);
    void decodeOperation(DxbcCodeSlice code);
    
    void decodeComponentSelection(DxbcRegister& reg, uint32_t token);
    void decodeOperandExtensions(DxbcCodeSlice& code, DxbcRegister& reg, uint32_t token);
    void decodeOperandImmediates(DxbcCodeSlice& code, DxbcRegister& reg);
    void decodeOperandIndex(DxbcCodeSlice& code, DxbcRegister& reg, uint32_t token);
    
    void decodeRegister(DxbcCodeSlice& code, DxbcRegister& reg, DxbcScalarType type);
    void decodeImm32(DxbcCodeSlice& code, DxbcImmediate& imm, DxbcScalarType type);
    
    void decodeOperand(DxbcCodeSlice& code, const DxbcInstOperandFormat& format);
    
  };
  
}