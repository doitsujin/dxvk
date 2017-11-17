#pragma once

#include "spirv_code_buffer.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V module
   * 
   * This class generates a code buffer containing a full
   * SPIR-V shader module. Ensures that the module layout
   * is valid, as defined in the SPIR-V 1.0 specification,
   * section 2.4 "Logical Layout of a Module".
   */
  class SpirvModule {
    
  public:
    
    SpirvModule();
    ~SpirvModule();
    
    SpirvCodeBuffer compile() const;
    
    uint32_t allocateId();
    
    void enableCapability(
            spv::Capability         capability);
    
    void addEntryPoint(
            uint32_t                entryPointId,
            spv::ExecutionModel     executionModel,
      const char*                   name,
            uint32_t                interfaceCount,
      const uint32_t*               interfaceIds);
    
    void setMemoryModel(
            spv::AddressingModel    addressModel,
            spv::MemoryModel        memoryModel);
    
    void enableEarlyFragmentTests(
            uint32_t                entryPointId);
    
    void setLocalSize(
            uint32_t                entryPointId,
            uint32_t                x,
            uint32_t                y,
            uint32_t                z);
    
    void setDebugName(
            uint32_t                expressionId,
      const char*                   debugName);
    
    void setDebugMemberName(
            uint32_t                structId,
            uint32_t                memberId,
      const char*                   debugName);
    
    uint32_t constBool(
            bool                    v);
    
    uint32_t consti32(
            int32_t                 v);
    
    uint32_t consti64(
            int64_t                 v);
    
    uint32_t constu32(
            uint32_t                v);
    
    uint32_t constu64(
            uint64_t                v);
    
    uint32_t constf32(
            float                   v);
    
    uint32_t constf64(
            double                  v);
    
    uint32_t constComposite(
            uint32_t                typeId,
            uint32_t                constCount,
      const uint32_t*               constIds);
    
    void decorateBlock(
            uint32_t                object);
    
    void decorateBuiltIn(
            uint32_t                object,
            spv::BuiltIn            builtIn);
    
    void decorateComponent(
            uint32_t                object,
            uint32_t                location);
    
    void decorateLocation(
            uint32_t                object,
            uint32_t                location);
    
    void memberDecorateBuiltIn(
            uint32_t                structId,
            uint32_t                memberId,
            spv::BuiltIn            builtIn);
    
    uint32_t defVoidType();
    
    uint32_t defBoolType();
    
    uint32_t defIntType(
            uint32_t                width,
            uint32_t                isSigned);
    
    uint32_t defFloatType(
            uint32_t                width);
    
    uint32_t defVectorType(
            uint32_t                elementType,
            uint32_t                elementCount);
    
    uint32_t defMatrixType(
            uint32_t                columnType,
            uint32_t                columnCount);
    
    uint32_t defArrayType(
            uint32_t                typeId,
            uint32_t                length);
    
    uint32_t defRuntimeArrayType(
            uint32_t                typeId);
    
    uint32_t defFunctionType(
            uint32_t                returnType,
            uint32_t                argCount,
      const uint32_t*               argTypes);
    
    uint32_t defStructType(
            uint32_t                memberCount,
      const uint32_t*               memberTypes);
    
    uint32_t defPointerType(
            uint32_t                variableType,
            spv::StorageClass       storageClass);
    
    uint32_t newVar(
            uint32_t                pointerType,
            spv::StorageClass       storageClass);
    
    void functionBegin(
            uint32_t                returnType,
            uint32_t                functionId,
            uint32_t                functionType,
      spv::FunctionControlMask      functionControl);
    
    uint32_t functionParameter(
            uint32_t                parameterType);
    
    void functionEnd();
    
    uint32_t opAccessChain(
            uint32_t                resultType,
            uint32_t                composite,
            uint32_t                indexCount,
      const uint32_t*               indexArray);
    
    uint32_t opBitcast(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opCompositeExtract(
            uint32_t                resultType,
            uint32_t                composite,
            uint32_t                indexCount,
      const uint32_t*               indexArray);
    
    uint32_t opCompositeInsert(
            uint32_t                resultType,
            uint32_t                object,
            uint32_t                composite,
            uint32_t                indexCount,
      const uint32_t*               indexArray);
    
    uint32_t opVectorShuffle(
            uint32_t                resultType,
            uint32_t                vectorLeft,
            uint32_t                vectorRight,
            uint32_t                indexCount,
      const uint32_t*               indexArray);
    
    uint32_t opSNegate(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opFNegate(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opSAbs(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opFAbs(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opIAdd(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFAdd(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFClamp(
            uint32_t                resultType,
            uint32_t                x,
            uint32_t                minVal,
            uint32_t                maxVal);
    
    uint32_t opFunctionCall(
            uint32_t                resultType,
            uint32_t                functionId,
            uint32_t                argCount,
      const uint32_t*               argIds);
    
    void opLabel(
            uint32_t                labelId);
    
    uint32_t opLoad(
            uint32_t                typeId,
            uint32_t                pointerId);
    
    void opStore(
            uint32_t                pointerId,
            uint32_t                valueId);
    
    void opReturn();
    
  private:
    
    uint32_t m_id             = 1;
    uint32_t m_instExtGlsl450 = 0;
    
    SpirvCodeBuffer m_capabilities;
    SpirvCodeBuffer m_instExt;
    SpirvCodeBuffer m_memoryModel;
    SpirvCodeBuffer m_entryPoints;
    SpirvCodeBuffer m_execModeInfo;
    SpirvCodeBuffer m_debugNames;
    SpirvCodeBuffer m_annotations;
    SpirvCodeBuffer m_typeConstDefs;
    SpirvCodeBuffer m_variables;
    SpirvCodeBuffer m_code;
    
    uint32_t defType(
            spv::Op                 op, 
            uint32_t                argCount,
      const uint32_t*               argIds);
    
    void instImportGlsl450();
    
  };
  
}