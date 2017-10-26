#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(DxbcProgramVersion version)
  : m_version(version) {
    m_entryPointId = m_module.allocateId();
    
    this->declareCapabilities();
    this->declareMemoryModel();
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  bool DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    const DxbcOpcodeToken token = ins.token();
    
    switch (token.opcode()) {
      case DxbcOpcode::DclThreadGroup: {
        m_module.setLocalSize(
          m_entryPointId,
          ins.getArgWord(0),
          ins.getArgWord(1),
          ins.getArgWord(2));
      } return true;
      
      default:
        Logger::err(str::format("DXBC: unhandled instruction: ",
          static_cast<uint32_t>(token.opcode())));
        return false;
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    return new DxvkShader(m_version.shaderStage(),
      m_module.compile(), 0, nullptr);
  }
  
  
  void DxbcCompiler::declareCapabilities() {
    m_module.enableCapability(spv::CapabilityShader);
    
    switch (m_version.type()) {
      case DxbcProgramType::GeometryShader:
        m_module.enableCapability(spv::CapabilityGeometry);
        break;
        
      case DxbcProgramType::HullShader:
      case DxbcProgramType::DomainShader:
        m_module.enableCapability(spv::CapabilityTessellation);
        break;
        
      default:
        break;
    }
  }
  
  
  void DxbcCompiler::declareMemoryModel() {
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
  }
  
}