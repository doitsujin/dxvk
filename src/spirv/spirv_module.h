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
    
    void setExecutionMode(
            uint32_t                entryPointId,
            spv::ExecutionMode      executionMode);
    
    void setLocalSize(
            uint32_t                entryPointId,
            uint32_t                x,
            uint32_t                y,
            uint32_t                z);
    
    void setOutputVertices(
            uint32_t                entryPointId,
            uint32_t                vertexCount);
    
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
    
    void decorate(
            uint32_t                object,
            spv::Decoration         decoration);
    
    void decorateArrayStride(
            uint32_t                object,
            uint32_t                stride);
    
    void decorateBinding(
            uint32_t                object,
            uint32_t                binding);
    
    void decorateBlock(
            uint32_t                object);
    
    void decorateBuiltIn(
            uint32_t                object,
            spv::BuiltIn            builtIn);
    
    void decorateComponent(
            uint32_t                object,
            uint32_t                location);
    
    void decorateDescriptorSet(
            uint32_t                object,
            uint32_t                set);
    
    void decorateLocation(
            uint32_t                object,
            uint32_t                location);
    
    void memberDecorateBuiltIn(
            uint32_t                structId,
            uint32_t                memberId,
            spv::BuiltIn            builtIn);
    
    void memberDecorateOffset(
            uint32_t                structId,
            uint32_t                memberId,
            uint32_t                offset);
    
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
    
    uint32_t defArrayTypeUnique(
            uint32_t                typeId,
            uint32_t                length);
    
    uint32_t defRuntimeArrayType(
            uint32_t                typeId);
    
    uint32_t defRuntimeArrayTypeUnique(
            uint32_t                typeId);
    
    uint32_t defFunctionType(
            uint32_t                returnType,
            uint32_t                argCount,
      const uint32_t*               argTypes);
    
    uint32_t defStructType(
            uint32_t                memberCount,
      const uint32_t*               memberTypes);
    
    uint32_t defStructTypeUnique(
            uint32_t                memberCount,
      const uint32_t*               memberTypes);
    
    uint32_t defPointerType(
            uint32_t                variableType,
            spv::StorageClass       storageClass);
    
    uint32_t defSamplerType();
    
    uint32_t defImageType(
            uint32_t                sampledType,
            spv::Dim                dimensionality,
            uint32_t                depth,
            uint32_t                arrayed,
            uint32_t                multisample,
            uint32_t                sampled,
            spv::ImageFormat        format);
    
    uint32_t defSampledImageType(
            uint32_t                imageType);
    
    uint32_t newVar(
            uint32_t                pointerType,
            spv::StorageClass       storageClass);
    
    uint32_t newVarInit(
            uint32_t                pointerType,
            spv::StorageClass       storageClass,
            uint32_t                initialValue);
    
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
    
    uint32_t opBitwiseAnd(
            uint32_t                resultType,
            uint32_t                operand1,
            uint32_t                operand2);
    
    uint32_t opBitwiseOr(
            uint32_t                resultType,
            uint32_t                operand1,
            uint32_t                operand2);
    
    uint32_t opBitwiseXor(
            uint32_t                resultType,
            uint32_t                operand1,
            uint32_t                operand2);
    
    uint32_t opNot(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opConvertFtoS(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opConvertFtoU(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opConvertStoF(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opConvertUtoF(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opCompositeConstruct(
            uint32_t                resultType,
            uint32_t                valueCount,
      const uint32_t*               valueArray);
    
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
    
    uint32_t opDpdx(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opDpdy(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opDpdxCoarse(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opDpdyCoarse(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opDpdxFine(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opDpdyFine(
            uint32_t                resultType,
            uint32_t                operand);
    
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
    
    uint32_t opSDiv(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opUDiv(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opSRem(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opUMod(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFDiv(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opIMul(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFMul(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFFma(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b,
            uint32_t                c);
    
    uint32_t opFMax(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFMin(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opSMax(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opSMin(
            uint32_t                resultType,
            uint32_t                a,
            uint32_t                b);
    
    uint32_t opFClamp(
            uint32_t                resultType,
            uint32_t                x,
            uint32_t                minVal,
            uint32_t                maxVal);
    
    uint32_t opIEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opINotEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opSLessThan(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opSLessThanEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opSGreaterThan(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opSGreaterThanEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opFOrdEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opFOrdNotEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opFOrdLessThan(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opFOrdLessThanEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opFOrdGreaterThan(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opFOrdGreaterThanEqual(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opDot(
            uint32_t                resultType,
            uint32_t                vector1,
            uint32_t                vector2);
    
    uint32_t opSin(
            uint32_t                resultType,
            uint32_t                vector);
    
    uint32_t opCos(
            uint32_t                resultType,
            uint32_t                vector);
    
    uint32_t opSqrt(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opInverseSqrt(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opExp2(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opLog2(
            uint32_t                resultType,
            uint32_t                operand);
    
    uint32_t opSelect(
            uint32_t                resultType,
            uint32_t                condition,
            uint32_t                operand1,
            uint32_t                operand2);
    
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
    
    uint32_t opSampledImage(
            uint32_t                resultType,
            uint32_t                image,
            uint32_t                sampler);
    
    uint32_t opImageSampleImplicitLod(
            uint32_t                resultType,
            uint32_t                sampledImage,
            uint32_t                coordinates);
    
    void opLoopMerge(
            uint32_t                mergeBlock,
            uint32_t                continueTarget,
            uint32_t                loopControl);
    
    void opSelectionMerge(
            uint32_t                mergeBlock,
            uint32_t                selectionControl);
    
    void opBranch(
            uint32_t                label);
    
    void opBranchConditional(
            uint32_t                condition,
            uint32_t                trueLabel,
            uint32_t                falseLabel);
    
    void opReturn();
    
    void opKill();
    
    void opEmitVertex();
    
    void opEndPrimitive();
    
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