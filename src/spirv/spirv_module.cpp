#include "spirv_module.h"

namespace dxvk {
  
  SpirvModule:: SpirvModule() {
    this->instImportGlsl450();
  }
  
  
  SpirvModule::~SpirvModule() {
    
  }
  
  
  SpirvCodeBuffer SpirvModule::compile() const {
    SpirvCodeBuffer result;
    result.putHeader(m_id);
    result.append(m_capabilities);
    result.append(m_instExt);
    result.append(m_memoryModel);
    result.append(m_entryPoints);
    result.append(m_execModeInfo);
    result.append(m_debugNames);
    result.append(m_annotations);
    result.append(m_typeConstDefs);
    result.append(m_variables);
    result.append(m_code);
    return result;
  }
  
  
  uint32_t SpirvModule::allocateId() {
    return m_id++;
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
    m_entryPoints.putIns  (spv::OpEntryPoint, 3 + m_entryPoints.strLen(name) + interfaceCount);
    m_entryPoints.putWord (executionModel);
    m_entryPoints.putWord (entryPointId);
    m_entryPoints.putStr  (name);
    
    for (uint32_t i = 0; i < interfaceCount; i++)
      m_entryPoints.putWord(interfaceIds[i]);
  }
  
  
  void SpirvModule::setMemoryModel(
          spv::AddressingModel    addressModel,
          spv::MemoryModel        memoryModel) {
    m_memoryModel.putIns  (spv::OpMemoryModel, 3);
    m_memoryModel.putWord (addressModel);
    m_memoryModel.putWord (memoryModel);
  }
  
  
  void SpirvModule::enableEarlyFragmentTests(
          uint32_t                entryPointId) {
    m_execModeInfo.putIns (spv::OpExecutionMode, 3);
    m_execModeInfo.putWord(entryPointId);
    m_execModeInfo.putWord(spv::ExecutionModeEarlyFragmentTests);
  }
  
  
  void SpirvModule::setLocalSize(
          uint32_t                entryPointId,
          uint32_t                x,
          uint32_t                y,
          uint32_t                z) {
    m_execModeInfo.putIns  (spv::OpExecutionMode, 6);
    m_execModeInfo.putWord (entryPointId);
    m_execModeInfo.putWord (spv::ExecutionModeLocalSize);
    m_execModeInfo.putInt32(x);
    m_execModeInfo.putInt32(y);
    m_execModeInfo.putInt32(z);
  }
  
  
  void SpirvModule::setOriginUpperLeft(
          uint32_t                entryPointId) {
    m_execModeInfo.putIns (spv::OpExecutionMode, 3);
    m_execModeInfo.putWord(entryPointId);
    m_execModeInfo.putWord(spv::ExecutionModeOriginUpperLeft);
  }
  
  
  void SpirvModule::setDebugName(
          uint32_t                expressionId,
    const char*                   debugName) {
    m_debugNames.putIns (spv::OpName, 2 + m_debugNames.strLen(debugName));
    m_debugNames.putWord(expressionId);
    m_debugNames.putStr (debugName);
  }
  
  
  void SpirvModule::setDebugMemberName(
          uint32_t                structId,
          uint32_t                memberId,
    const char*                   debugName) {
    m_debugNames.putIns (spv::OpMemberName, 3 + m_debugNames.strLen(debugName));
    m_debugNames.putWord(structId);
    m_debugNames.putWord(memberId);
    m_debugNames.putStr (debugName);
  }
  
  
  uint32_t SpirvModule::constBool(
          bool                    v) {
    uint32_t typeId   = this->defBoolType();
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (v ? spv::OpConstantTrue : spv::OpConstantFalse, 3);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    return resultId;
  }
  
  
  uint32_t SpirvModule::consti32(
          int32_t                 v) {
    uint32_t typeId   = this->defIntType(32, 1);
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstant, 4);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    m_typeConstDefs.putInt32(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::consti64(
          int64_t                 v) {
    uint32_t typeId   = this->defIntType(64, 1);
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstant, 5);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    m_typeConstDefs.putInt64(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constu32(
          uint32_t                v) {
    uint32_t typeId   = this->defIntType(32, 0);
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstant, 4);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    m_typeConstDefs.putInt32(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constu64(
          uint64_t                v) {
    uint32_t typeId   = this->defIntType(64, 0);
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstant, 5);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    m_typeConstDefs.putInt64(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constf32(
          float                   v) {
    uint32_t typeId   = this->defFloatType(32);
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstant, 4);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    m_typeConstDefs.putFloat32(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constf64(
          double                  v) {
    uint32_t typeId   = this->defFloatType(64);
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstant, 5);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    m_typeConstDefs.putFloat64(v);
    return resultId;
  }
  
  
  uint32_t SpirvModule::constComposite(
          uint32_t                typeId,
          uint32_t                constCount,
    const uint32_t*               constIds) {
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns  (spv::OpConstantComposite, 3 + constCount);
    m_typeConstDefs.putWord (typeId);
    m_typeConstDefs.putWord (resultId);
    
    for (uint32_t i = 0; i < constCount; i++)
      m_typeConstDefs.putWord(constIds[i]);
    return resultId;
  }
    
  
  void SpirvModule::decorate(
          uint32_t                object,
          spv::Decoration         decoration) {
    m_annotations.putIns  (spv::OpDecorate, 3);
    m_annotations.putWord (object);
    m_annotations.putWord (decoration);
  }
  
  
  void SpirvModule::decorateArrayStride(
          uint32_t                object,
          uint32_t                stride) {
    m_annotations.putIns  (spv::OpDecorate, 4);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationArrayStride);
    m_annotations.putInt32(stride);
  }
  
  
  void SpirvModule::decorateBinding(
          uint32_t                object,
          uint32_t                binding) {
    m_annotations.putIns  (spv::OpDecorate, 4);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationBinding);
    m_annotations.putInt32(binding);
  }
  
  
  void SpirvModule::decorateBlock(uint32_t object) {
    m_annotations.putIns  (spv::OpDecorate, 3);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationBlock);
  }
  
  
  void SpirvModule::decorateBuiltIn(
          uint32_t                object,
          spv::BuiltIn            builtIn) {
    m_annotations.putIns  (spv::OpDecorate, 4);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationBuiltIn);
    m_annotations.putWord (builtIn);
  }
  
  
  void SpirvModule::decorateComponent(
          uint32_t                object,
          uint32_t                location) {
    m_annotations.putIns  (spv::OpDecorate, 4);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationComponent);
    m_annotations.putInt32(location);
  }
  
  
  void SpirvModule::decorateDescriptorSet(
          uint32_t                object,
          uint32_t                set) {
    m_annotations.putIns  (spv::OpDecorate, 4);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationDescriptorSet);
    m_annotations.putInt32(set);
  }
  
  
  void SpirvModule::decorateLocation(
          uint32_t                object,
          uint32_t                location) {
    m_annotations.putIns  (spv::OpDecorate, 4);
    m_annotations.putWord (object);
    m_annotations.putWord (spv::DecorationLocation);
    m_annotations.putInt32(location);
  }
  
  
  void SpirvModule::memberDecorateBuiltIn(
          uint32_t                structId,
          uint32_t                memberId,
          spv::BuiltIn            builtIn) {
    m_annotations.putIns  (spv::OpMemberDecorate, 5);
    m_annotations.putWord (structId);
    m_annotations.putWord (memberId);
    m_annotations.putWord (spv::DecorationBuiltIn);
    m_annotations.putWord (builtIn);
  }
  
  
  void SpirvModule::memberDecorateOffset(
          uint32_t                structId,
          uint32_t                memberId,
          uint32_t                offset) {
    m_annotations.putIns  (spv::OpMemberDecorate, 5);
    m_annotations.putWord (structId);
    m_annotations.putWord (memberId);
    m_annotations.putWord (spv::DecorationOffset);
    m_annotations.putWord (offset);
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
  
  
  uint32_t SpirvModule::defArrayTypeUnique(
          uint32_t                typeId,
          uint32_t                length) {
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns (spv::OpTypeArray, 4);
    m_typeConstDefs.putWord(resultId);
    m_typeConstDefs.putWord(typeId);
    m_typeConstDefs.putWord(length);
    return resultId;
  }
  
  
  uint32_t SpirvModule::defRuntimeArrayType(
          uint32_t                typeId) {
    std::array<uint32_t, 1> args = { typeId };
    
    return this->defType(spv::OpTypeRuntimeArray,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defRuntimeArrayTypeUnique(
          uint32_t                typeId) {
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns (spv::OpTypeRuntimeArray, 3);
    m_typeConstDefs.putWord(resultId);
    m_typeConstDefs.putWord(typeId);
    return resultId;
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
  
  
  uint32_t SpirvModule::defStructTypeUnique(
          uint32_t                memberCount,
    const uint32_t*               memberTypes) {
    uint32_t resultId = this->allocateId();
    
    m_typeConstDefs.putIns (spv::OpTypeStruct, 2 + memberCount);
    m_typeConstDefs.putWord(resultId);
    
    for (uint32_t i = 0; i < memberCount; i++)
      m_typeConstDefs.putWord(memberTypes[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::defPointerType(
          uint32_t                variableType,
          spv::StorageClass       storageClass) {
    std::array<uint32_t, 2> args = {
      storageClass,
      variableType,
    };
    
    return this->defType(spv::OpTypePointer,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defSamplerType() {
    return this->defType(spv::OpTypeSampler, 0, nullptr);
  }
  
  
  uint32_t SpirvModule::defImageType(
          uint32_t                sampledType,
          spv::Dim                dimensionality,
          uint32_t                depth,
          uint32_t                arrayed,
          uint32_t                multisample,
          uint32_t                sampled,
          spv::ImageFormat        format) {
    std::array<uint32_t, 7> args = {
      sampledType, dimensionality,
      depth, arrayed, multisample,
      sampled, format
    };
    
    return this->defType(spv::OpTypeImage,
      args.size(), args.data());
  }
  
  
  uint32_t SpirvModule::defSampledImageType(
          uint32_t                imageType) {
    return this->defType(spv::OpTypeSampledImage, 1, &imageType);
  }
  
  
  uint32_t SpirvModule::newVar(
          uint32_t                pointerType,
          spv::StorageClass       storageClass) {
    uint32_t resultId = this->allocateId();
    
    m_variables.putIns  (spv::OpVariable, 4);
    m_variables.putWord (pointerType);
    m_variables.putWord (resultId);
    m_variables.putWord (storageClass);
    return resultId;
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
  
  
  uint32_t SpirvModule::opAccessChain(
          uint32_t                resultType,
          uint32_t                composite,
          uint32_t                indexCount,
    const uint32_t*               indexArray) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpAccessChain, 4 + indexCount);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(composite);
    
    for (uint32_t i = 0; i < indexCount; i++)
      m_code.putInt32(indexArray[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opBitcast(
          uint32_t                resultType,
          uint32_t                operand) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpBitcast, 4);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(operand);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opCompositeExtract(
          uint32_t                resultType,
          uint32_t                composite,
          uint32_t                indexCount,
    const uint32_t*               indexArray) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpCompositeExtract, 4 + indexCount);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(composite);
    
    for (uint32_t i = 0; i < indexCount; i++)
      m_code.putInt32(indexArray[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opCompositeInsert(
          uint32_t                resultType,
          uint32_t                object,
          uint32_t                composite,
          uint32_t                indexCount,
    const uint32_t*               indexArray) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpCompositeInsert, 5 + indexCount);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(object);
    m_code.putWord(composite);
    
    for (uint32_t i = 0; i < indexCount; i++)
      m_code.putInt32(indexArray[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opVectorShuffle(
          uint32_t                resultType,
          uint32_t                vectorLeft,
          uint32_t                vectorRight,
          uint32_t                indexCount,
    const uint32_t*               indexArray) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpVectorShuffle, 5 + indexCount);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(vectorLeft);
    m_code.putWord(vectorRight);
    
    for (uint32_t i = 0; i < indexCount; i++)
      m_code.putInt32(indexArray[i]);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opSNegate(
          uint32_t                resultType,
          uint32_t                operand) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpSNegate, 4);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(operand);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opFNegate(
          uint32_t                resultType,
          uint32_t                operand) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpFNegate, 4);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(operand);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opSAbs(
          uint32_t                resultType,
          uint32_t                operand) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpExtInst, 6);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(m_instExtGlsl450);
    m_code.putWord(spv::GLSLstd450SAbs);
    m_code.putWord(operand);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opFAbs(
          uint32_t                resultType,
          uint32_t                operand) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpExtInst, 6);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(m_instExtGlsl450);
    m_code.putWord(spv::GLSLstd450FAbs);
    m_code.putWord(operand);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opIAdd(
          uint32_t                resultType,
          uint32_t                a,
          uint32_t                b) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpIAdd, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(a);
    m_code.putWord(b);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opFAdd(
          uint32_t                resultType,
          uint32_t                a,
          uint32_t                b) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpFAdd, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(a);
    m_code.putWord(b);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opIMul(
          uint32_t                resultType,
          uint32_t                a,
          uint32_t                b) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpIMul, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(a);
    m_code.putWord(b);
    return resultId;
  }
  
    
  uint32_t SpirvModule::opFMul(
          uint32_t                resultType,
          uint32_t                a,
          uint32_t                b) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpFMul, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(a);
    m_code.putWord(b);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opFClamp(
          uint32_t                resultType,
          uint32_t                x,
          uint32_t                minVal,
          uint32_t                maxVal) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpExtInst, 8);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(m_instExtGlsl450);
    m_code.putWord(spv::GLSLstd450FClamp);
    m_code.putWord(x);
    m_code.putWord(minVal);
    m_code.putWord(maxVal);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opDot(
          uint32_t                resultType,
          uint32_t                vector1,
          uint32_t                vector2) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpDot, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(vector1);
    m_code.putWord(vector2);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opInverseSqrt(
          uint32_t                resultType,
          uint32_t                x) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpExtInst, 6);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(m_instExtGlsl450);
    m_code.putWord(spv::GLSLstd450InverseSqrt);
    m_code.putWord(x);
    return resultId;
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
  
  
  void SpirvModule::opLabel(uint32_t labelId) {
    m_code.putIns (spv::OpLabel, 2);
    m_code.putWord(labelId);
  }
  
  
  uint32_t SpirvModule::opLoad(
          uint32_t                typeId,
          uint32_t                pointerId) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpLoad, 4);
    m_code.putWord(typeId);
    m_code.putWord(resultId);
    m_code.putWord(pointerId);
    return resultId;
  }
  
  
  void SpirvModule::opStore(
          uint32_t                pointerId,
          uint32_t                valueId) {
    m_code.putIns (spv::OpStore, 3);
    m_code.putWord(pointerId);
    m_code.putWord(valueId);
  }
  
  
  uint32_t SpirvModule::opSampledImage(
          uint32_t                resultType,
          uint32_t                image,
          uint32_t                sampler) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpSampledImage, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(image);
    m_code.putWord(sampler);
    return resultId;
  }
  
  
  uint32_t SpirvModule::opImageSampleImplicitLod(
          uint32_t                resultType,
          uint32_t                sampledImage,
          uint32_t                coordinates) {
    uint32_t resultId = this->allocateId();
    
    m_code.putIns (spv::OpImageSampleImplicitLod, 5);
    m_code.putWord(resultType);
    m_code.putWord(resultId);
    m_code.putWord(sampledImage);
    m_code.putWord(coordinates);
    return resultId;
  }
  
  
  void SpirvModule::opReturn() {
    m_code.putIns (spv::OpReturn, 1);
  }
  
  
  uint32_t SpirvModule::defType(
          spv::Op                 op, 
          uint32_t                argCount,
    const uint32_t*               argIds) {
    // Since the type info is stored in the code buffer,
    // we can use the code buffer to look up type IDs as
    // well. Result IDs are always stored as argument 1.
    for (auto ins : m_typeConstDefs) {
      bool match = ins.opCode() == op;
      
      for (uint32_t i = 0; i < argCount && match; i++)
        match &= ins.arg(2 + i) == argIds[i];
      
      if (match)
        return ins.arg(1);
    }
    
    // Type not yet declared, create a new one.
    uint32_t resultId = this->allocateId();
    m_typeConstDefs.putIns (op, 2 + argCount);
    m_typeConstDefs.putWord(resultId);
    
    for (uint32_t i = 0; i < argCount; i++)
      m_typeConstDefs.putWord(argIds[i]);
    return resultId;
  }
  
  
  void SpirvModule::instImportGlsl450() {
    m_instExtGlsl450 = this->allocateId();
    const char* name = "GLSL.std.450";
    
    m_instExt.putIns (spv::OpExtInstImport, 2 + m_instExt.strLen(name));
    m_instExt.putWord(m_instExtGlsl450);
    m_instExt.putStr (name);
  }
  
}