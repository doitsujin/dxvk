#pragma once

#include "dxbc_enums.h"
#include "dxbc_type.h"

namespace dxvk {
  
  constexpr size_t DxbcMaxInterfaceRegs = 32;
  constexpr size_t DxbcMaxOperandCount  = 8;
  
  enum class DxbcOperandKind {
    DstReg, ///< Destination register
    SrcReg, ///< Source register
    Imm32,  ///< Constant number
  };
  
  enum class DxbcInstClass {
    Declaration,      ///< Interface or resource declaration
    TextureSample,    ///< Texture sampling instruction
    VectorAlu,        ///< Component-wise vector instructions
    VectorCmp,        ///< Component-wise vector comparison
    VectorDot,        ///< Dot product instruction
    VectorSinCos,     ///< Sine and Cosine instruction
    ControlFlow,      ///< Control flow instructions
    Undefined,        ///< Instruction code not defined
  };
  
  struct DxbcInstOperandFormat {
    DxbcOperandKind kind;
    DxbcScalarType  type;
  };
  
  struct DxbcInstFormat {
    uint32_t              operandCount      = 0;
    DxbcInstClass         instructionClass  = DxbcInstClass::Undefined;
    DxbcInstOperandFormat operands[DxbcMaxOperandCount];
  };
  
  DxbcInstFormat dxbcInstructionFormat(DxbcOpcode opcode);
  
}