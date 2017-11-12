#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(
    const DxbcProgramVersion& version)
  : m_gen(DxbcCodeGen::create(version)) { }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    const DxbcOpcodeToken token = ins.token();
    
    switch (token.opcode()) {
      case DxbcOpcode::DclGlobalFlags:
        return this->dclGlobalFlags(ins);
      
      case DxbcOpcode::DclInput:
      case DxbcOpcode::DclInputSiv:
      case DxbcOpcode::DclInputSgv:
      case DxbcOpcode::DclInputPs:
      case DxbcOpcode::DclInputPsSiv:
      case DxbcOpcode::DclInputPsSgv:
        return this->dclInput(ins);
      
      case DxbcOpcode::DclOutput:
      case DxbcOpcode::DclOutputSiv:
      case DxbcOpcode::DclOutputSgv:
        return this->dclOutput(ins);
      
      case DxbcOpcode::DclTemps:
        return this->dclTemps(ins);
      
      default:
        Logger::err(str::format(
          "DxbcCompiler::processInstruction: Unhandled opcode: ",
          token.opcode()));
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    return m_gen->finalize();
  }
  
  
  void DxbcCompiler::dclGlobalFlags(const DxbcInstruction& ins) {
    
  }
  
  
  void DxbcCompiler::dclInput(const DxbcInstruction& ins) {
    
  }
  
  
  void DxbcCompiler::dclOutput(const DxbcInstruction& ins) {
    
  }
  
  
  void DxbcCompiler::dclTemps(const DxbcInstruction& ins) {
    m_gen->dclTemps(ins.arg(0));
  }
  
}