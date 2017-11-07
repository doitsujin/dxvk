#include "dxbc_compiler.h"
#include "dxbc_names.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(
          DxbcProgramVersion  version)
  : m_version(version) {
    
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    return new DxvkShader(m_version.shaderStage(),
      m_module.compile(), 0, nullptr);
  }
  
}