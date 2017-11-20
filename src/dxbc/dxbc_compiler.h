#pragma once

#include "./gen/dxbc_gen_common.h"

namespace dxvk {
  
  /**
   * \brief DXBC compiler
   * 
   * Interprets DXBC instructions and generates
   * SPIR-V code for the appropriate shader type.
   */
  class DxbcCompiler {
    
  public:
    
    DxbcCompiler(
      const DxbcProgramVersion& version);
    ~DxbcCompiler();
    
    void processInstruction(
      const DxbcInstruction& ins);
    
    SpirvCodeBuffer finalize();
    
  private:
    
    Rc<DxbcCodeGen> m_gen;
    
    void dclGlobalFlags(
      const DxbcInstruction&  ins);
    
    void dclInterfaceVar(
      const DxbcInstruction&  ins);
    
    void dclTemps(
      const DxbcInstruction&  ins);
    
    void opAdd(
      const DxbcInstruction&  ins);
    
    void opMov(
      const DxbcInstruction&  ins);
    
    void opRet(
      const DxbcInstruction&  ins);
    
    DxbcComponentMask getDstOperandMask(
      const DxbcOperand&      operand);
    
    DxbcPointer getTempOperandPtr(
      const DxbcOperand&      operand);
    
    DxbcPointer getInterfaceOperandPtr(
      const DxbcOperand&      operand);
    
    DxbcPointer getOperandPtr(
      const DxbcOperand&      operand);
    
    DxbcValue selectOperandComponents(
      const DxbcOperandToken& opToken,
      const DxbcValue&        opValue,
            DxbcComponentMask dstMask);
    
    DxbcValue applyOperandModifiers(
            DxbcValue             value,
            DxbcOperandModifiers  modifiers);
    
    DxbcValue applyResultModifiers(
            DxbcValue             value,
            DxbcOpcodeControl     control);
            
    DxbcValue loadOperand(
      const DxbcOperand&      operand,
            DxbcComponentMask dstMask,
            DxbcScalarType    dstType);
    
    void storeOperand(
      const DxbcOperand&      operand,
            DxbcValue         value,
            DxbcComponentMask mask);
    
  };
  
}