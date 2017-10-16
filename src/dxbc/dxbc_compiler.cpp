#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(DxbcProgramVersion version)
  : m_version(version) {
    this->enableCapability(spv::CapabilityShader);
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
    codeBuffer.append(m_spirvCapabilities);
    codeBuffer.append(m_spirvProgramCode);
    
    return new DxvkShader(this->shaderStage(),
      std::move(codeBuffer), 0, nullptr);
  }
  
  
  VkShaderStageFlagBits DxbcCompiler::shaderStage() const {
    switch (m_version.type()) {
      case DxbcProgramType::PixelShader    : return VK_SHADER_STAGE_FRAGMENT_BIT;
      case DxbcProgramType::VertexShader   : return VK_SHADER_STAGE_VERTEX_BIT;
      case DxbcProgramType::GeometryShader : return VK_SHADER_STAGE_GEOMETRY_BIT;
      case DxbcProgramType::HullShader     : return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
      case DxbcProgramType::DomainShader   : return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
      case DxbcProgramType::ComputeShader  : return VK_SHADER_STAGE_COMPUTE_BIT;
    }
    
    throw DxvkError("DxbcCompiler::shaderStage: Unknown program type");
  }
  
  
  void DxbcCompiler::enableCapability(spv::Capability cap) {
    if (m_capabilities.find(cap) == m_capabilities.end()) {
      m_spirvCapabilities.putIns (spv::OpCapability, 2);
      m_spirvCapabilities.putWord(cap);
      m_capabilities.insert(cap);
    }
  }
  
}