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
  
  enum class DxbcRegComponentCount : uint32_t {
    c0 = 0,
    c1 = 1,
    c4 = 2,
  };
  
  enum class DxbcRegModifier : uint32_t {
    Neg           = 0,
    Abs           = 1,
  };
  
  using DxbcRegModifiers = Flags<DxbcRegModifier>;
  
  
  struct DxbcRegIndex {
    DxbcRegister*     relReg;
    int32_t           offset;
  };
  
  
  struct DxbcRegister {
    DxbcOperandType       type;
    DxbcScalarType        dataType;
    DxbcRegComponentCount componentCount;
    
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
  
  
  struct DxbcOpModifiers {
    bool saturate;
    bool precise;
  };
  
  
  struct DxbcShaderOpcodeControls {
    DxbcZeroTest          zeroTest;
    DxbcSyncFlags         syncFlags;
    DxbcResourceDim       resourceDim;
    DxbcResinfoType       resinfoType;
    DxbcInterpolationMode interpolation;
  };
  
  
  struct DxbcShaderSampleControls {
    int u, v, w;
  };
  
  
  union DxbcImmediate {
    uint32_t u32;
    uint64_t u64;
  };
  
  
  /**
   * \brief Shader instruction
   */
  struct DxbcShaderInstruction {
    DxbcOpcode                op;
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