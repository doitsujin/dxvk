#pragma once

#include "dxbc_gen_common.h"

namespace dxvk {
  
  /**
   * \brief Pixel shader code generator
   */
  class DxbcPsCodeGen : public DxbcCodeGen {
    
  public:
    
    DxbcPsCodeGen(
      const Rc<DxbcIsgn>& osgn);
    ~DxbcPsCodeGen();
    
    void dclInterfaceVar(
            DxbcOperandType   regType,
            uint32_t          regId,
            uint32_t          regDim,
            DxbcComponentMask regMask,
            DxbcSystemValue   sv);
    
    DxbcPointer ptrInterfaceVar(
            DxbcOperandType   regType,
            uint32_t          regId);
    
    DxbcPointer ptrInterfaceVarIndexed(
            DxbcOperandType   regType,
            uint32_t          regId,
      const DxbcValue&        index);
    
    SpirvCodeBuffer finalize() final;
    
  private:
    
    uint32_t m_function = 0;
    uint32_t m_psIn     = 0;
    
    DxbcPointer m_svPosition;
    
    std::array<DxbcPointer, 32> m_vRegs;
    std::array<DxbcPointer, 8>  m_oRegs;
    std::array<DxbcPointer, 8>  m_psOut;
    
    void dclSvInputReg(DxbcSystemValue sv);
    
    void prepareSvInputs();
    void prepareSvOutputs();
    
    DxbcPointer getPsInPtr(uint32_t id);
    
  };
  
}