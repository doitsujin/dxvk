#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(DxbcProgramVersion version)
  : m_version(version) {
    m_spvCapabilities.enable(spv::CapabilityShader);
    
    m_spvEntryPoints.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
    
    auto id = m_counter.nextId();
    m_spvEntryPoints.addEntryPoint(id,
      spv::ExecutionModelGLCompute,
      "main", 0, nullptr);
    m_spvEntryPoints.setLocalSize(id, 64, 1, 1);
    auto ft = m_spvTypeInfo.typeFunction(m_counter,
      m_spvTypeInfo.typeVoid(m_counter), 0, nullptr);
    m_spvCode.putIns  (spv::OpFunction, 5);
    m_spvCode.putWord (m_spvTypeInfo.typeVoid(m_counter));
    m_spvCode.putWord (id);
    m_spvCode.putWord (0);
    m_spvCode.putWord (ft);
    m_spvCode.putIns  (spv::OpFunctionEnd, 1);
    m_entryPointId = m_counter.nextId();
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(DxbcInstruction ins) {
    Logger::info(str::format(
      static_cast<uint32_t>(ins.opcode())));
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    DxvkSpirvCodeBuffer codeBuffer;
    codeBuffer.putHeader(m_counter.numIds());
    codeBuffer.append(m_spvCapabilities.code());
    codeBuffer.append(m_spvEntryPoints.code());
    codeBuffer.append(m_spvTypeInfo.code());
    codeBuffer.append(m_spvCode);
    
    return new DxvkShader(m_version.shaderStage(),
      std::move(codeBuffer), 0, nullptr);
  }
  
}