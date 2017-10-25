#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(DxbcProgramVersion version)
  : m_version(version) {
    m_entryPointId = m_counter.nextId();
    
    this->declareCapabilities();
    this->declareMemoryModel();
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  bool DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    const DxbcOpcodeToken token = ins.token();
    
    switch (token.opcode()) {
      case DxbcOpcode::DclThreadGroup: {
        m_spvEntryPoints.setLocalSize(
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
    SpirvCodeBuffer codeBuffer;
    codeBuffer.putHeader(m_counter.numIds());
    codeBuffer.append(m_spvCapabilities.code());
    codeBuffer.append(m_spvEntryPoints.code());
    codeBuffer.append(m_spvDebugInfo.code());
    codeBuffer.append(m_spvDecorations.code());
    codeBuffer.append(m_spvTypeInfo.code());
    codeBuffer.append(m_spvConstants.code());
    codeBuffer.append(m_spvVariables.code());
    codeBuffer.append(m_spvCode);
    
    return new DxvkShader(m_version.shaderStage(),
      std::move(codeBuffer), 0, nullptr);
  }
  
  
  void DxbcCompiler::declareCapabilities() {
    m_spvCapabilities.enable(spv::CapabilityShader);
    
    switch (m_version.type()) {
      case DxbcProgramType::GeometryShader:
        m_spvCapabilities.enable(spv::CapabilityGeometry);
        break;
        
      case DxbcProgramType::HullShader:
      case DxbcProgramType::DomainShader:
        m_spvCapabilities.enable(spv::CapabilityTessellation);
        break;
        
      default:
        break;
    }
  }
  
  
  void DxbcCompiler::declareMemoryModel() {
    m_spvEntryPoints.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
  }
  
}