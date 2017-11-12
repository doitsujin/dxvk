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
    
    Rc<DxvkShader> finalize();
    
  private:
    
    Rc<DxbcCodeGen> m_gen;
    
    void dclGlobalFlags(
      const DxbcInstruction& ins);
    
    void dclInput(
      const DxbcInstruction& ins);
    
    void dclOutput(
      const DxbcInstruction& ins);
    
    void dclTemps(
      const DxbcInstruction& ins);
    
  };
  
}