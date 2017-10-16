#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(DxbcProgramVersion version) {
    
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(DxbcInstruction ins) {
    Logger::info(str::format(
      static_cast<uint32_t>(ins.opcode())));
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    return new DxvkShader(VK_SHADER_STAGE_COMPUTE_BIT,
      DxvkSpirvCodeBuffer(), 0, nullptr);
  }
  
}