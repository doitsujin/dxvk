#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(DxbcProgramVersion version)
  : m_version(version) {
    m_entryPointId = m_module.allocateId();
    
    this->declareCapabilities();
    this->declareMemoryModel();
    
    m_typeVoid      = m_module.defVoidType();
    m_typeFunction  = m_module.defFunctionType(m_typeVoid, 0, nullptr);
    
    m_module.functionBegin(m_typeVoid,
      m_entryPointId, m_typeFunction,
      spv::FunctionControlMaskNone);
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  bool DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    const DxbcOpcodeToken token = ins.token();
    
    switch (token.opcode()) {
      case DxbcOpcode::DclTemps: {
        this->dclTemps(ins.arg(0));
      } return true;
      
      case DxbcOpcode::DclThreadGroup: {
        m_module.setLocalSize(
          m_entryPointId,
          ins.arg(0),
          ins.arg(1),
          ins.arg(2));
      } return true;
      
      default:
        Logger::err(str::format("DXBC: unhandled instruction: ",
          static_cast<uint32_t>(token.opcode())));
        return false;
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    m_module.functionEnd();
    
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
  
  void DxbcCompiler::dclTemps(uint32_t n) {
    // Temporaries are treated as untyped 4x32-bit vectors.
    uint32_t u32Type = m_module.defIntType(32, 0);
    uint32_t regType = m_module.defVectorType(u32Type, 4);
    uint32_t ptrType = m_module.defPointerType(regType, spv::StorageClassPrivate);
    
    for (uint32_t i = 0; i < n; i++) {
      DxbcRegTypeR reg;
      reg.varType = regType;
      reg.ptrType = ptrType;
      reg.varId   = m_module.newVar(ptrType, spv::StorageClassPrivate);
      m_rRegs.push_back(reg);
      
      m_module.setDebugName(reg.varId,
        str::format("r", i).c_str());
    }
  }
  
}