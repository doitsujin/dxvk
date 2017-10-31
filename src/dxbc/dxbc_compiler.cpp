#include "dxbc_compiler.h"

namespace dxvk {
  
  DxbcCompiler::DxbcCompiler(
          DxbcProgramVersion  version,
    const Rc<DxbcIsgn>&       inputSig,
    const Rc<DxbcIsgn>&       outputSig)
  : m_version   (version),
    m_inputSig  (inputSig),
    m_outputSig (outputSig) {
    m_entryPointId = m_module.allocateId();
    
    this->declareCapabilities();
    this->declareMemoryModel();
    
    m_typeVoid      = m_module.defVoidType();
    m_typeFunction  = m_module.defFunctionType(m_typeVoid, 0, nullptr);
    
    m_module.functionBegin(m_typeVoid,
      m_entryPointId, m_typeFunction,
      spv::FunctionControlMaskNone);
    
    // TODO implement proper control flow
    m_module.opLabel(m_module.allocateId());
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(const DxbcInstruction& ins) {
    const DxbcOpcodeToken token = ins.token();
    
    switch (token.opcode()) {
      case DxbcOpcode::DclGlobalFlags:
        return this->dclGlobalFlags(ins);
        
      case DxbcOpcode::DclInput:
        return this->dclInput(ins);
      
      case DxbcOpcode::DclOutputSiv:
        return this->dclOutputSiv(ins);
      
      case DxbcOpcode::DclTemps:
        return this->dclTemps(ins);
      
      case DxbcOpcode::DclThreadGroup:
        return this->dclThreadGroup(ins);
      
      case DxbcOpcode::Mov:
        return this->opMov(ins);
      
      case DxbcOpcode::Ret:
        return this->opRet(ins);
      
      default:
        throw DxvkError(str::format("DXBC: Unhandled instruction: ", ins.token().opcode()));
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    m_module.functionEnd();
    
    m_module.addEntryPoint(m_entryPointId,
      m_version.executionModel(), "main",
      m_interfaces.size(),
      m_interfaces.data());
    
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
  
  
  void DxbcCompiler::dclGlobalFlags(const DxbcInstruction& ins) {
    const DxbcGlobalFlags flags(ins.token().control());
    
    if (!flags.test(DxbcGlobalFlag::RefactoringAllowed))
      m_useRestrictedMath = true;
    
    if (flags.test(DxbcGlobalFlag::DoublePrecision))
      m_module.enableCapability(spv::CapabilityFloat64);
    
    if (flags.test(DxbcGlobalFlag::EarlyFragmentTests))
      m_module.enableEarlyFragmentTests(m_entryPointId);
    
    // Raw and structured buffers are supported regardless
    // of whether the corresponding flag is set or not.
  }
  
  
  void DxbcCompiler::dclInput(const DxbcInstruction& ins) {
    const DxbcOperand       operand = ins.operand(0);
    const DxbcOperandToken  token   = operand.token();
    
  }
  
  
  void DxbcCompiler::dclOutputSiv(const DxbcInstruction& ins) {
    Logger::err("DXBC: dclOutputSiv: Not implemented yet");
  }
  
  
  void DxbcCompiler::dclTemps(const DxbcInstruction& ins) {
    // Temporaries are treated as untyped 4x32-bit vectors.
    const DxbcValueType   regType(DxbcScalarType::Uint32, 4);
    const DxbcPointerType ptrType(regType, spv::StorageClassPrivate);
    const uint32_t        ptrTypeId = this->getPointerTypeId(ptrType);
    
    for (uint32_t i = 0; i < ins.arg(0); i++) {
      DxbcPointer reg;
      reg.type    = ptrType;
      reg.typeId  = ptrTypeId;
      reg.valueId = m_module.newVar(ptrTypeId, spv::StorageClassPrivate);
      m_rRegs.push_back(reg);
      
      m_module.setDebugName(reg.valueId,
        str::format("r", i).c_str());
    }
  }
  
  
  void DxbcCompiler::dclThreadGroup(const DxbcInstruction& ins) {
    m_module.setLocalSize(m_entryPointId,
      ins.arg(0), ins.arg(1), ins.arg(2));
  }
  
  
  void DxbcCompiler::opMov(const DxbcInstruction& ins) {
    const DxbcOperand dstOp = ins.operand(0);
    const DxbcOperand srcOp = ins.operand(dstOp.length());
    
    DxbcValueType dstType(DxbcScalarType::Uint32, 1);
    this->loadOperand(srcOp, dstType);
    
    Logger::err("DXBC: mov: Not implemented yet");
  }
  
  
  void DxbcCompiler::opRet(const DxbcInstruction& ins) {
    // TODO implement proper control flow
    m_module.opReturn();
  }
  
  
  uint32_t DxbcCompiler::getScalarTypeId(const DxbcScalarType& type) {
    switch (type) {
      case DxbcScalarType::Uint32 : return m_module.defIntType(32, 0);
      case DxbcScalarType::Uint64 : return m_module.defIntType(64, 0);
      case DxbcScalarType::Sint32 : return m_module.defIntType(32, 1);
      case DxbcScalarType::Sint64 : return m_module.defIntType(64, 1);
      case DxbcScalarType::Float32: return m_module.defFloatType(32);
      case DxbcScalarType::Float64: return m_module.defFloatType(64);
    }
    
    throw DxvkError("DXBC: Invalid scalar type");
  }
  
  
  uint32_t DxbcCompiler::getValueTypeId(const DxbcValueType& type) {
    const uint32_t scalarTypeId = this->getScalarTypeId(type.componentType);
    
    return type.componentCount > 1
      ? m_module.defVectorType(scalarTypeId, type.componentCount)
      : scalarTypeId;
  }
  
  
  uint32_t DxbcCompiler::getPointerTypeId(const DxbcPointerType& type) {
    return m_module.defPointerType(
      this->getValueTypeId(type.valueType),
      type.storageClass);
  }
  
  
  DxbcValue DxbcCompiler::loadPointer(const DxbcPointer& pointer) {
    DxbcValue result;
    result.type    = pointer.type.valueType;
    result.typeId  = this->getValueTypeId(result.type);
    result.valueId = m_module.opLoad(result.typeId, pointer.valueId);
    return result;
  }
  
  
  DxbcValue DxbcCompiler::loadOperand(
    const DxbcOperand&        operand,
    const DxbcValueType&      type) {
    const DxbcOperandToken token = operand.token();
    
    DxbcValue result;
    
    switch (token.type()) {
      
      case DxbcOperandType::Imm32: {
        const uint32_t componentCount = token.numComponents();
        
        result.type   = DxbcValueType(DxbcScalarType::Uint32, componentCount);
        result.typeId = this->getValueTypeId(result.type);
        
        if (componentCount == 1) {
          result.valueId = m_module.constu32(operand.imm32(0));
        } else {
          std::array<uint32_t, 4> constIds;
          
          for (uint32_t i = 0; i < componentCount; i++)
            constIds.at(i) = m_module.constu32(operand.imm32(i));
          
          result.valueId = m_module.constComposite(
            result.typeId, componentCount, constIds.data());
        }
      } break;
      
      case DxbcOperandType::Temp: {
        const DxbcOperandIndex index = operand.index(0);
        result = this->loadPointer(m_rRegs.at(index.immPart()));
      } break;
      
      case DxbcOperandType::Input: {
        const DxbcOperandIndex index = operand.index(0);
        result = this->loadPointer(m_vRegs.at(index.immPart()));
      } break;
      
      case DxbcOperandType::Output: {
        const DxbcOperandIndex index = operand.index(0);
        result = this->loadPointer(m_oRegs.at(index.immPart()));
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler::loadOperandRegister: Unhandled operand type: ",
          token.type()));
    }
    
    return result;
  }
  
  
  void DxbcCompiler::storePointer(
    const DxbcPointer&        pointer,
    const DxbcValue&          value) {
    m_module.opStore(pointer.valueId, value.valueId);
  }
  
  
  void DxbcCompiler::storeOperand(
    const DxbcOperand&        operand,
    const DxbcValueType&      srcType,
          uint32_t            srcValue) {
    
  }
  
}