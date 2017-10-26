#include "spirv_module.h"

namespace dxvk {
  
  SpirvModule:: SpirvModule() { }
  SpirvModule::~SpirvModule() { }
  
  
  SpirvCodeBuffer SpirvModule::compile() const {
    SpirvCodeBuffer result;
    result.putHeader(m_id);
    result.append(m_capabilities);
    result.append(m_memoryModel);
    result.append(m_entryPoints);
    result.append(m_execModeInfo);
    result.append(m_debugNames);
    result.append(m_annotations);
    result.append(m_typeDefs);
    result.append(m_constDefs);
    result.append(m_variables);
    result.append(m_code);
    return result;
  }
  
  
  uint32_t SpirvModule::allocateId() {
    return ++m_id;
  }
  
  
  void SpirvModule::enableCapability(
          spv::Capability         capability) {
    // Scan the generated instructions to check
    // whether we already enabled the capability.
    for (auto ins : m_capabilities) {
      if (ins.opCode() == spv::OpCapability && ins.arg(1) == capability)
        return;
    }
    
    m_capabilities.putIns (spv::OpCapability, 2);
    m_capabilities.putWord(capability);
  }
  
  
  void SpirvModule::addEntryPoint(
          uint32_t                entryPointId,
          spv::ExecutionModel     executionModel,
    const char*                   name,
          uint32_t                interfaceCount,
    const uint32_t*               interfaceIds) {
    
  }
  
  
  void SpirvModule::setMemoryModel(
          spv::AddressingModel    addressModel,
          spv::MemoryModel        memoryModel) {
    
  }
  
  
  void SpirvModule::enableEarlyFragmentTests(
          uint32_t                entryPointId) {
    
  }
  
  
  void SpirvModule::setLocalSize(
          uint32_t                entryPointId,
          uint32_t                x,
          uint32_t                y,
          uint32_t                z) {
    
  }
  
  
  void SpirvModule::setDebugName(
          uint32_t                expressionId,
    const char*                   debugName) {
    
  }
  
  
  uint32_t SpirvModule::constBool(
          bool                    v) {
    uint32_t typeId   = this->defBoolType();
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (v ? spv::OpConstantTrue : spv::OpConstantFalse, 3);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    return resultId;
  }
  
  
  uint32_t SpirvModule::consti32(
          int32_t                 v) {
    uint32_t typeId   = this->defIntType(32, 1);
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstant, 4);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    m_constDefs.putInt32(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::consti64(
          int64_t                 v) {
    uint32_t typeId   = this->defIntType(64, 1);
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstant, 5);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    m_constDefs.putInt64(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constu32(
          uint32_t                v) {
    uint32_t typeId   = this->defIntType(32, 0);
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstant, 4);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    m_constDefs.putInt32(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constu64(
          uint64_t                v) {
    uint32_t typeId   = this->defIntType(64, 0);
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstant, 5);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    m_constDefs.putInt64(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constf32(
          float                   v) {
    uint32_t typeId   = this->defFloatType(32);
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstant, 4);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    m_constDefs.putFloat32(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constf64(
          double                  v) {
    uint32_t typeId   = this->defFloatType(64);
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstant, 5);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    m_constDefs.putFloat64(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constComposite(
          uint32_t                typeId,
          uint32_t                constCount,
    const uint32_t*               constIds) {
    uint32_t resultId = this->allocateId();
    
    m_constDefs.putIns  (spv::OpConstantComposite, 3 + constCount);
    m_constDefs.putWord (typeId);
    m_constDefs.putWord (resultId);
    
    for (uint32_t i = 0; i < constCount; i++)
      m_constDefs.putWord(constIds[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::defVoidType() {
    return this->defType(spv::OpTypeVoid, 0, nullptr);
  }
  
  
  uint32_t SpirvModule::defBoolType() {
    return this->defType(spv::OpTypeBool, 0, nullptr);
  }
  
  
  uint32_t SpirvModule::defIntType(
          uint32_t                width,
          uint32_t                isSigned) {
    std::array<uint32_t, 2> args = { width, isSigned };
    return this->defType(spv::OpTypeInt,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defFloatType(
          uint32_t                width) {
    std::array<uint32_t, 1> args = { width };
    return this->defType(spv::OpTypeFloat,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defVectorType(
          uint32_t                elementType,
          uint32_t                elementCount) {
    std::array<uint32_t, 2> args = {
      elementType,
      elementCount
    };
    
    return this->defType(spv::OpTypeVector,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defMatrixType(
          uint32_t                columnType,
          uint32_t                columnCount) {
    std::array<uint32_t, 2> args = {
      columnType,
      columnCount
    };
    
    return this->defType(spv::OpTypeMatrix,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defArrayType(
          uint32_t                typeId,
          uint32_t                length) {
    std::array<uint32_t, 2> args = { typeId, length };
    
    return this->defType(spv::OpTypeArray,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defRuntimeArrayType(
          uint32_t                typeId) {
    std::array<uint32_t, 1> args = { typeId };
    
    return this->defType(spv::OpTypeRuntimeArray,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defFunctionType(
          uint32_t                returnType,
          uint32_t                argCount,
    const uint32_t*               argTypes) {
    std::vector<uint32_t> args;
    args.push_back(returnType);
    
    for (uint32_t i = 0; i < argCount; i++)
      args.push_back(argTypes[i]);
    
    return this->defType(spv::OpTypeFunction,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defStructType(
          uint32_t                memberCount,
    const uint32_t*               memberTypes) {
    return this->defType(spv::OpTypeStruct,
      memberCount, memberTypes);
  }
  
  
  uint32_t SpirvModule::defPointerType(
          uint32_t                variableType,
          spv::StorageClass       storageClass) {
    std::array<uint32_t, 2> args = {
      variableType,
      storageClass,
    };
    
    return this->defType(spv::OpTypePointer,
      args.size(), args.data());
  }
  
  
  void SpirvModule::functionBegin(
          uint32_t                returnType,
          uint32_t                functionId,
          uint32_t                functionType,
    spv::FunctionControlMask      functionControl) {
    m_code.putIns (spv::OpFunction, 5);
    m_code.putWord(returnType);
    m_code.putWord(functionId);
    m_code.putWord(functionControl);
    m_code.putWord(functionType);
  }
  
  
  uint32_t SpirvModule::functionParameter(
          uint32_t                parameterType) {
    uint32_t parameterId = this->allocateId();
    
    m_code.putIns (spv::OpFunctionParameter, 3);
    m_code.putWord(parameterType);
    m_code.putWord(parameterId);
    return parameterId;
  }
  
  
  void SpirvModule::functionEnd() {
    m_code.putIns (spv::OpFunctionEnd, 1);
  }
  
  
  uint32_t SpirvModule::opFunctionCall(
          uint32_t                resultType,
          uint32_t                functionId,
          uint32_t                argCount,
    const uint32_t*               argIds) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpFunctionCall, 4 + argCount);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(functionId);
    
    for (uint32_t i = 0; i < argCount; i++)
      m_code.putWord(argIds[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::defType(
          spv::Op                 op, 
          uint32_t                argCount,
    const uint32_t*               argIds) {
    // Since the type info is stored in the code buffer,
    // we can use the code buffer to look up type IDs as
    // well. Result IDs are always stored as argument 1.
    for (auto ins : m_typeDefs) {
      bool match = ins.opCode() == op;
      
      for (uint32_t i = 0; i < argCount && match; i++)
        match &= ins.arg(2 + i) == argIds[i];
      
      if (match)
        return ins.arg(1);
    }
    
    // Type not yet declared, create a new one.
    uint32_t resultId = this->allocateId();
    m_typeDefs.putIns (op, 2 + argCount);
    m_typeDefs.putWord(resultId);
    
    for (uint32_t i = 0; i < argCount; i++)
      m_typeDefs.putWord(argIds[i]);
    return resultId;
  }
  
}