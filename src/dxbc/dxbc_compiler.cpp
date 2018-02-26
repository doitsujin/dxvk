#include "dxbc_compiler.h"

namespace dxvk {
  
  constexpr uint32_t PerVertex_Position  = 0;
  constexpr uint32_t PerVertex_CullDist  = 1;
  constexpr uint32_t PerVertex_ClipDist  = 2;
  
  constexpr uint32_t PushConstant_InstanceId = 0;
  
  DxbcCompiler::DxbcCompiler(
    const DxbcOptions&        options,
    const DxbcProgramVersion& version,
    const Rc<DxbcIsgn>&       isgn,
    const Rc<DxbcIsgn>&       osgn)
  : m_options (options),
    m_version (version),
    m_isgn    (isgn),
    m_osgn    (osgn) {
    // Declare an entry point ID. We'll need it during the
    // initialization phase where the execution mode is set.
    m_entryPointId = m_module.allocateId();
    
    // Set the memory model. This is the same for all shaders.
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
    
    // Make sure our interface registers are clear
    for (uint32_t i = 0; i < DxbcMaxInterfaceRegs; i++) {
      m_ps.oTypes.at(i).ctype  = DxbcScalarType::Float32;
      m_ps.oTypes.at(i).ccount = 0;
      
      m_vRegs.at(i) = 0;
      m_oRegs.at(i) = 0;
    }
    
    this->emitInit();
  }
  
  
  DxbcCompiler::~DxbcCompiler() {
    
  }
  
  
  void DxbcCompiler::processInstruction(const DxbcShaderInstruction& ins) {
    switch (ins.opClass) {
      case DxbcInstClass::Declaration:
        return this->emitDcl(ins);
      
      case DxbcInstClass::CustomData:
        return this->emitCustomData(ins);
        
      case DxbcInstClass::Atomic:
        return this->emitAtomic(ins);
        
      case DxbcInstClass::AtomicCounter:
        return this->emitAtomicCounter(ins);
        
      case DxbcInstClass::Barrier:
        return this->emitBarrier(ins);
        
      case DxbcInstClass::BitExtract:
        return this->emitBitExtract(ins);
        
      case DxbcInstClass::BitInsert:
        return this->emitBitInsert(ins);
        
      case DxbcInstClass::BufferQuery:
        return this->emitBufferQuery(ins);
        
      case DxbcInstClass::BufferLoad:
        return this->emitBufferLoad(ins);
        
      case DxbcInstClass::BufferStore:
        return this->emitBufferStore(ins);
        
      case DxbcInstClass::ConvertFloat16:
        return this->emitConvertFloat16(ins);
        
      case DxbcInstClass::ControlFlow:
        return this->emitControlFlow(ins);
        
      case DxbcInstClass::GeometryEmit:
        return this->emitGeometryEmit(ins);
      
      case DxbcInstClass::TextureQuery:
        return this->emitTextureQuery(ins);
        
      case DxbcInstClass::TextureQueryLod:
        return this->emitTextureQueryLod(ins);
        
      case DxbcInstClass::TextureQueryMs:
        return this->emitTextureQueryMs(ins);
        
      case DxbcInstClass::TextureFetch:
        return this->emitTextureFetch(ins);
        
      case DxbcInstClass::TextureGather:
        return this->emitTextureGather(ins);
        
      case DxbcInstClass::TextureSample:
        return this->emitTextureSample(ins);
        
      case DxbcInstClass::TypedUavLoad:
        return this->emitTypedUavLoad(ins);
        
      case DxbcInstClass::TypedUavStore:
        return this->emitTypedUavStore(ins);
        
      case DxbcInstClass::VectorAlu:
        return this->emitVectorAlu(ins);
        
      case DxbcInstClass::VectorCmov:
        return this->emitVectorCmov(ins);
        
      case DxbcInstClass::VectorCmp:
        return this->emitVectorCmp(ins);
        
      case DxbcInstClass::VectorDeriv:
        return this->emitVectorDeriv(ins);
        
      case DxbcInstClass::VectorDot:
        return this->emitVectorDot(ins);
        
      case DxbcInstClass::VectorIdiv:
        return this->emitVectorIdiv(ins);
        
      case DxbcInstClass::VectorImul:
        return this->emitVectorImul(ins);
        
      case DxbcInstClass::VectorShift:
        return this->emitVectorShift(ins);
        
      case DxbcInstClass::VectorSinCos:
        return this->emitVectorSinCos(ins);
        
      default:
        Logger::warn(
          str::format("DxbcCompiler: Unhandled opcode class: ",
          ins.op));
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler::finalize() {
    // Define the actual 'main' function of the shader
    m_module.functionBegin(
      m_module.defVoidType(),
      m_entryPointId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
    
    // Depending on the shader type, this will prepare
    // input registers, call various shader functions
    // and write back the output registers.
    switch (m_version.type()) {
      case DxbcProgramType::VertexShader:   this->emitVsFinalize(); break;
      case DxbcProgramType::GeometryShader: this->emitGsFinalize(); break;
      case DxbcProgramType::PixelShader:    this->emitPsFinalize(); break;
      case DxbcProgramType::ComputeShader:  this->emitCsFinalize(); break;
      default: throw DxvkError("DxbcCompiler: Unsupported program type");
    }
    
    // End main function
    m_module.opReturn();
    m_module.functionEnd();
    
    // Declare the entry point, we now have all the
    // information we need, including the interfaces
    m_module.addEntryPoint(m_entryPointId,
      m_version.executionModel(), "main",
      m_entryPointInterfaces.size(),
      m_entryPointInterfaces.data());
    m_module.setDebugName(m_entryPointId, "main");
    
    // Create the shader module object
    return new DxvkShader(
      m_version.shaderStage(),
      m_resourceSlots.size(),
      m_resourceSlots.data(),
      m_interfaceSlots,
      m_module.compile());
  }
  
  
  void DxbcCompiler::emitDcl(const DxbcShaderInstruction& ins) {
    switch (ins.op) {
      case DxbcOpcode::DclGlobalFlags:
        return this->emitDclGlobalFlags(ins);
        
      case DxbcOpcode::DclIndexRange:
        return;  // not needed for anything
        
      case DxbcOpcode::DclTemps:
        return this->emitDclTemps(ins);
        
      case DxbcOpcode::DclIndexableTemp:
        return this->emitDclIndexableTemp(ins);
        
      case DxbcOpcode::DclInput:
      case DxbcOpcode::DclInputSgv:
      case DxbcOpcode::DclInputSiv:
      case DxbcOpcode::DclInputPs:
      case DxbcOpcode::DclInputPsSgv:
      case DxbcOpcode::DclInputPsSiv:
      case DxbcOpcode::DclOutput:
      case DxbcOpcode::DclOutputSgv:
      case DxbcOpcode::DclOutputSiv:
        return this->emitDclInterfaceReg(ins);
        
      case DxbcOpcode::DclConstantBuffer:
        return this->emitDclConstantBuffer(ins);
        
      case DxbcOpcode::DclSampler:
        return this->emitDclSampler(ins);
      
      case DxbcOpcode::DclStream:
        return this->emitDclStream(ins);
        
      case DxbcOpcode::DclUavTyped:
      case DxbcOpcode::DclResource:
        return this->emitDclResourceTyped(ins);
        
      case DxbcOpcode::DclUavRaw:
      case DxbcOpcode::DclResourceRaw:
      case DxbcOpcode::DclUavStructured:
      case DxbcOpcode::DclResourceStructured:
        return this->emitDclResourceRawStructured(ins);
      
      case DxbcOpcode::DclThreadGroupSharedMemoryRaw:
      case DxbcOpcode::DclThreadGroupSharedMemoryStructured:
        return this->emitDclThreadGroupSharedMemory(ins);
        
      case DxbcOpcode::DclGsInputPrimitive:
        return this->emitDclGsInputPrimitive(ins);
        
      case DxbcOpcode::DclGsOutputPrimitiveTopology:
        return this->emitDclGsOutputTopology(ins);
        
      case DxbcOpcode::DclMaxOutputVertexCount:
        return this->emitDclMaxOutputVertexCount(ins);
        
      case DxbcOpcode::DclThreadGroup:
        return this->emitDclThreadGroup(ins);
      
      default:
        Logger::warn(
          str::format("DxbcCompiler: Unhandled opcode: ",
          ins.op));
    }
  }
  
  
  void DxbcCompiler::emitDclGlobalFlags(const DxbcShaderInstruction& ins) {
    const DxbcGlobalFlags flags = ins.controls.globalFlags;
    
    if (flags.test(DxbcGlobalFlag::EarlyFragmentTests))
      m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeEarlyFragmentTests);
  }
  
  
  void DxbcCompiler::emitDclTemps(const DxbcShaderInstruction& ins) {
    // dcl_temps has one operand:
    //    (imm0) Number of temp registers
    const uint32_t oldCount = m_rRegs.size();
    const uint32_t newCount = ins.imm[0].u32;
    
    if (newCount > oldCount) {
      m_rRegs.resize(newCount);
      
      DxbcRegisterInfo info;
      info.type.ctype   = DxbcScalarType::Float32;
      info.type.ccount  = 4;
      info.type.alength = 0;
      info.sclass       = spv::StorageClassPrivate;
      
      for (uint32_t i = oldCount; i < newCount; i++) {
        const uint32_t varId = this->emitNewVariable(info);
        m_module.setDebugName(varId, str::format("r", i).c_str());
        m_rRegs.at(i) = varId;
      }
    }
  }
  
  
  void DxbcCompiler::emitDclIndexableTemp(const DxbcShaderInstruction& ins) {
    // dcl_indexable_temps has three operands:
    //    (imm0) Array register index (x#)
    //    (imm1) Number of vectors stored in the array
    //    (imm2) Component count of each individual vector
    DxbcRegisterInfo info;
    info.type.ctype   = DxbcScalarType::Float32;
    info.type.ccount  = ins.imm[2].u32;
    info.type.alength = ins.imm[1].u32;
    info.sclass       = spv::StorageClassPrivate;
    
    const uint32_t regId = ins.imm[0].u32;
    
    if (regId >= m_xRegs.size())
      m_xRegs.resize(regId + 1);
    
    m_xRegs.at(regId).ccount = info.type.ccount;
    m_xRegs.at(regId).varId  = emitNewVariable(info);
    
    m_module.setDebugName(m_xRegs.at(regId).varId,
      str::format("x", regId).c_str());
  }
  
  
  void DxbcCompiler::emitDclInterfaceReg(const DxbcShaderInstruction& ins) {
    switch (ins.dst[0].type) {
      case DxbcOperandType::Input:
      case DxbcOperandType::Output: {
        // dcl_input and dcl_output instructions
        // have the following operands:
        //    (dst0) The register to declare
        //    (imm0) The system value (optional)
        uint32_t regDim = 0;
        uint32_t regIdx = 0;
        
        // In the vertex and fragment shader stage, the
        // operand indices will have the following format:
        //    (0) Register index
        // 
        // In other stages, the input and output registers
        // may be declared as arrays of a fixed size:
        //    (0) Array length
        //    (1) Register index
        if (ins.dst[0].idxDim == 2) {
          regDim = ins.dst[0].idx[0].offset;
          regIdx = ins.dst[0].idx[1].offset;
        } else if (ins.dst[0].idxDim == 1) {
          regIdx = ins.dst[0].idx[0].offset;
        } else {
          Logger::err(str::format(
            "DxbcCompiler: ", ins.op,
            ": Invalid index dimension"));
          return;
        }
        
        // This declaration may map an output register to a system
        // value. If that is the case, the system value type will
        // be stored in the second operand.
        const bool hasSv =
            ins.op == DxbcOpcode::DclInputSgv
          || ins.op == DxbcOpcode::DclInputSiv
          || ins.op == DxbcOpcode::DclInputPsSgv
          || ins.op == DxbcOpcode::DclInputPsSiv
          || ins.op == DxbcOpcode::DclOutputSgv
          || ins.op == DxbcOpcode::DclOutputSiv;
        
        DxbcSystemValue sv = DxbcSystemValue::None;
        
        if (hasSv)
          sv = static_cast<DxbcSystemValue>(ins.imm[0].u32);
        
        // In the pixel shader, inputs are declared with an
        // interpolation mode that is part of the op token.
        const bool hasInterpolationMode =
            ins.op == DxbcOpcode::DclInputPs
          || ins.op == DxbcOpcode::DclInputPsSiv;
        
        DxbcInterpolationMode im = DxbcInterpolationMode::Undefined;
        
        if (hasInterpolationMode)
          im = ins.controls.interpolation;
        
        // Declare the actual input/output variable
        switch (ins.op) {
          case DxbcOpcode::DclInput:
          case DxbcOpcode::DclInputSgv:
          case DxbcOpcode::DclInputSiv:
          case DxbcOpcode::DclInputPs:
          case DxbcOpcode::DclInputPsSgv:
          case DxbcOpcode::DclInputPsSiv:
            this->emitDclInput(regIdx, regDim, ins.dst[0].mask, sv, im);
            break;
          
          case DxbcOpcode::DclOutput:
          case DxbcOpcode::DclOutputSgv:
          case DxbcOpcode::DclOutputSiv:
            this->emitDclOutput(regIdx, regDim, ins.dst[0].mask, sv, im);
            break;
          
          default:
            Logger::err(str::format(
              "DxbcCompiler: Unexpected opcode: ",
              ins.op));
        }
      } break;
  
      case DxbcOperandType::InputThreadId: {
        m_cs.builtinGlobalInvocationId = emitNewBuiltinVariable({
          { DxbcScalarType::Uint32, 3, 0 },
          spv::StorageClassInput },
          spv::BuiltInGlobalInvocationId,
          "vThreadId");
      } break;
  
      case DxbcOperandType::InputThreadGroupId: {
        m_cs.builtinWorkgroupId = emitNewBuiltinVariable({
          { DxbcScalarType::Uint32, 3, 0 },
          spv::StorageClassInput },
          spv::BuiltInWorkgroupId,
          "vThreadGroupId");
      } break;
  
      case DxbcOperandType::InputThreadIdInGroup: {
        m_cs.builtinLocalInvocationId = emitNewBuiltinVariable({
          { DxbcScalarType::Uint32, 3, 0 },
          spv::StorageClassInput },
          spv::BuiltInLocalInvocationId,
          "vThreadIdInGroup");
      } break;
  
      case DxbcOperandType::InputThreadIndexInGroup: {
        m_cs.builtinLocalInvocationIndex = emitNewBuiltinVariable({
          { DxbcScalarType::Uint32, 1, 0 },
          spv::StorageClassInput },
          spv::BuiltInLocalInvocationIndex,
          "vThreadIndexInGroup");
      } break;
      
      case DxbcOperandType::InputCoverageMask: {
        m_module.enableCapability(spv::CapabilitySampleRateShading);
        m_ps.builtinSampleMaskIn = emitNewBuiltinVariable({
          { DxbcScalarType::Uint32, 1, 1 },
          spv::StorageClassInput },
          spv::BuiltInSampleMask,
          "vCoverage");
      } break;
      
      case DxbcOperandType::OutputCoverageMask: {
        m_module.enableCapability(spv::CapabilitySampleRateShading);
        m_ps.builtinSampleMaskOut = emitNewBuiltinVariable({
          { DxbcScalarType::Uint32, 1, 1 },
          spv::StorageClassOutput },
          spv::BuiltInSampleMask,
          "oMask");
      } break;
      
      case DxbcOperandType::OutputDepth: {
        m_module.setExecutionMode(m_entryPointId,
          spv::ExecutionModeDepthReplacing);
        m_ps.builtinDepth = emitNewBuiltinVariable({
          { DxbcScalarType::Float32, 1, 0 },
          spv::StorageClassOutput },
          spv::BuiltInFragDepth,
          "oDepth");
      } break;
      
      case DxbcOperandType::OutputDepthGe: {
        m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeDepthReplacing);
        m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeDepthGreater);
        m_ps.builtinDepth = emitNewBuiltinVariable({
          { DxbcScalarType::Float32, 1, 0 },
          spv::StorageClassOutput },
          spv::BuiltInFragDepth,
          "oDepthGe");
      } break;
      
      case DxbcOperandType::OutputDepthLe: {
        m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeDepthReplacing);
        m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeDepthLess);
        m_ps.builtinDepth = emitNewBuiltinVariable({
          { DxbcScalarType::Float32, 1, 0 },
          spv::StorageClassOutput },
          spv::BuiltInFragDepth,
          "oDepthLe");
      } break;
      
      default:
        Logger::err(str::format(
          "DxbcCompiler: Unsupported operand type declaration: ",
          ins.dst[0].type));
        
    }
  }
  
  
  void DxbcCompiler::emitDclInput(
          uint32_t                regIdx,
          uint32_t                regDim,
          DxbcRegMask             regMask,
          DxbcSystemValue         sv,
          DxbcInterpolationMode   im) {
    // Avoid declaring the same variable multiple times.
    // This may happen when multiple system values are
    // mapped to different parts of the same register.
    if (m_vRegs.at(regIdx) == 0 && sv == DxbcSystemValue::None) {
      const DxbcVectorType regType = getInputRegType(regIdx);
      
      DxbcRegisterInfo info;
      info.type.ctype   = regType.ctype;
      info.type.ccount  = regType.ccount;
      info.type.alength = regDim;
      info.sclass = spv::StorageClassInput;
      
      const uint32_t varId = emitNewVariable(info);
      
      m_module.decorateLocation(varId, regIdx);
      m_module.setDebugName(varId, str::format("v", regIdx).c_str());
      m_entryPointInterfaces.push_back(varId);
      
      m_vRegs.at(regIdx) = varId;
      
      // Interpolation mode, used in pixel shaders
      if (im == DxbcInterpolationMode::Constant)
        m_module.decorate(varId, spv::DecorationFlat);
      
      if (im == DxbcInterpolationMode::LinearCentroid
       || im == DxbcInterpolationMode::LinearNoPerspectiveCentroid)
        m_module.decorate(varId, spv::DecorationCentroid);
      
      if (im == DxbcInterpolationMode::LinearNoPerspective
       || im == DxbcInterpolationMode::LinearNoPerspectiveCentroid
       || im == DxbcInterpolationMode::LinearNoPerspectiveSample)
        m_module.decorate(varId, spv::DecorationNoPerspective);
      
      if (im == DxbcInterpolationMode::LinearSample
       || im == DxbcInterpolationMode::LinearNoPerspectiveSample)
        m_module.decorate(varId, spv::DecorationSample);
      
      // Declare the input slot as defined
      m_interfaceSlots.inputSlots |= 1u << regIdx;
    } else if (sv != DxbcSystemValue::None) {
      // Add a new system value mapping if needed
      m_vMappings.push_back({ regIdx, regMask, sv });
    }
  }
  
  
  void DxbcCompiler::emitDclOutput(
          uint32_t                regIdx,
          uint32_t                regDim,
          DxbcRegMask             regMask,
          DxbcSystemValue         sv,
          DxbcInterpolationMode   im) {
    // Avoid declaring the same variable multiple times.
    // This may happen when multiple system values are
    // mapped to different parts of the same register.
    if (m_oRegs.at(regIdx) == 0) {
      DxbcRegisterInfo info;
      info.type.ctype   = DxbcScalarType::Float32;
      info.type.ccount  = 4;
      info.type.alength = regDim;
      info.sclass = spv::StorageClassOutput;
      
      const uint32_t varId = this->emitNewVariable(info);
      
      m_module.decorateLocation(varId, regIdx);
      m_module.setDebugName(varId, str::format("o", regIdx).c_str());
      m_entryPointInterfaces.push_back(varId);
      
      m_oRegs.at(regIdx) = varId;
      
      // Declare the output slot as defined
      m_interfaceSlots.outputSlots |= 1u << regIdx;
    }
    
    
    // Add a new system value mapping if needed
    if (sv != DxbcSystemValue::None)
      m_oMappings.push_back({ regIdx, regMask, sv });
  }
  
  
  void DxbcCompiler::emitDclConstantBuffer(const DxbcShaderInstruction& ins) {
    // dcl_constant_buffer has one operand with two indices:
    //    (0) Constant buffer register ID (cb#)
    //    (1) Number of constants in the buffer
    const uint32_t bufferId     = ins.dst[0].idx[0].offset;
    const uint32_t elementCount = ins.dst[0].idx[1].offset;
    
    // Uniform buffer data is stored as a fixed-size array
    // of 4x32-bit vectors. SPIR-V requires explicit strides.
    const uint32_t arrayType = m_module.defArrayTypeUnique(
      getVectorTypeId({ DxbcScalarType::Float32, 4 }),
      m_module.constu32(elementCount));
    m_module.decorateArrayStride(arrayType, 16);
    
    // SPIR-V requires us to put that array into a
    // struct and decorate that struct as a block.
    const uint32_t structType = m_module.defStructTypeUnique(1, &arrayType);
    
    m_module.decorateBlock       (structType);
    m_module.memberDecorateOffset(structType, 0, 0);
    
    m_module.setDebugName        (structType, str::format("struct_cb", bufferId).c_str());
    m_module.setDebugMemberName  (structType, 0, "m");
    
    // Variable that we'll use to access the buffer
    const uint32_t varId = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);
    
    m_module.setDebugName(varId,
      str::format("cb", bufferId).c_str());
    
    // Compute the DXVK binding slot index for the buffer.
    // D3D11 needs to bind the actual buffers to this slot.
    const uint32_t bindingId = computeResourceSlotId(
      m_version.type(), DxbcBindingType::ConstantBuffer,
      bufferId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Declare a specialization constant which will
    // store whether or not the resource is bound.
    const uint32_t specConstId = m_module.specConstBool(true);
    m_module.decorateSpecId(specConstId, bindingId);
    m_module.setDebugName(specConstId,
      str::format("cb", bufferId, "_bound").c_str());
    
    DxbcConstantBuffer buf;
    buf.varId  = varId;
    buf.specId = specConstId;
    buf.size   = elementCount;
    m_constantBuffers.at(bufferId) = buf;
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    m_resourceSlots.push_back(resource);
  }
  
  
  void DxbcCompiler::emitDclSampler(const DxbcShaderInstruction& ins) {
    // dclSampler takes one operand:
    //    (dst0) The sampler register to declare
    const uint32_t samplerId = ins.dst[0].idx[0].offset;
    
    // The sampler type is opaque, but we still have to
    // define a pointer and a variable in oder to use it
    const uint32_t samplerType = m_module.defSamplerType();
    const uint32_t samplerPtrType = m_module.defPointerType(
      samplerType, spv::StorageClassUniformConstant);
    
    // Define the sampler variable
    const uint32_t varId = m_module.newVar(samplerPtrType,
      spv::StorageClassUniformConstant);
    m_module.setDebugName(varId,
      str::format("s", samplerId).c_str());
    
    m_samplers.at(samplerId).varId  = varId;
    m_samplers.at(samplerId).typeId = samplerType;
    
    // Compute binding slot index for the sampler
    const uint32_t bindingId = computeResourceSlotId(
      m_version.type(), DxbcBindingType::ImageSampler, samplerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    resource.view = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    m_resourceSlots.push_back(resource);
  }
  
  
  void DxbcCompiler::emitDclStream(const DxbcShaderInstruction& ins) {
    if (ins.dst[0].idx[0].offset != 0)
      Logger::err("Dxbc: Multiple streams not supported");
  }
  
  
  void DxbcCompiler::emitDclResourceTyped(const DxbcShaderInstruction& ins) {
    // dclResource takes two operands:
    //    (dst0) The resource register ID
    //    (imm0) The resource return type
    const uint32_t registerId = ins.dst[0].idx[0].offset;
    
    // We also handle unordered access views here
    const bool isUav = ins.op == DxbcOpcode::DclUavTyped;
    
    if (isUav) {
      m_module.enableCapability(spv::CapabilityStorageImageReadWithoutFormat);
      m_module.enableCapability(spv::CapabilityStorageImageWriteWithoutFormat);
    }
    
    // Defines the type of the resource (texture2D, ...)
    const DxbcResourceDim resourceType = ins.controls.resourceDim;
    
    // Defines the type of a read operation. DXBC has the ability
    // to define four different types whereas SPIR-V only allows
    // one, but in practice this should not be much of a problem.
    auto xType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.imm[0].u32, 0, 3));
    auto yType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.imm[0].u32, 4, 7));
    auto zType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.imm[0].u32, 8, 11));
    auto wType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.imm[0].u32, 12, 15));
    
    if ((xType != yType) || (xType != zType) || (xType != wType))
      Logger::warn("DxbcCompiler: dcl_resource: Ignoring resource return types");
    
    // Declare the actual sampled type
    const DxbcScalarType sampledType = [xType] {
      switch (xType) {
        case DxbcResourceReturnType::Float: return DxbcScalarType::Float32;
        case DxbcResourceReturnType::Sint:  return DxbcScalarType::Sint32;
        case DxbcResourceReturnType::Uint:  return DxbcScalarType::Uint32;
        default: throw DxvkError(str::format("DxbcCompiler: Invalid sampled type: ", xType));
      }
    }();
    
    const uint32_t sampledTypeId = getScalarTypeId(sampledType);
    
    // Declare the resource type
    // TODO test multisampled images
    const DxbcImageInfo typeInfo = [resourceType, isUav] () -> DxbcImageInfo {
      switch (resourceType) {
        case DxbcResourceDim::Buffer:         return { spv::DimBuffer, 0, 0, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture1D:      return { spv::Dim1D,     0, 0, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture1DArr:   return { spv::Dim1D,     1, 0, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture2D:      return { spv::Dim2D,     0, 0, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture2DArr:   return { spv::Dim2D,     1, 0, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture2DMs:    return { spv::Dim2D,     0, 1, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture2DMsArr: return { spv::Dim2D,     1, 1, isUav ? 2u : 1u };
        case DxbcResourceDim::Texture3D:      return { spv::Dim3D,     0, 0, isUav ? 2u : 1u };
        // Apps may bind cube maps to a slot that expects cube map arrays
        case DxbcResourceDim::TextureCube:    return { spv::DimCube,   1, 0, isUav ? 2u : 1u };
        case DxbcResourceDim::TextureCubeArr: return { spv::DimCube,   1, 0, isUav ? 2u : 1u };
        default: throw DxvkError(str::format("DxbcCompiler: Unsupported resource type: ", resourceType));
      }
    }();
    
    // Declare additional capabilities if necessary
    switch (resourceType) {
      case DxbcResourceDim::Buffer:         m_module.enableCapability(spv::CapabilityImageBuffer);    break;
      case DxbcResourceDim::Texture1D:      m_module.enableCapability(spv::CapabilityImage1D);        break;
      case DxbcResourceDim::Texture1DArr:   m_module.enableCapability(spv::CapabilityImage1D);        break;
      case DxbcResourceDim::TextureCube:    m_module.enableCapability(spv::CapabilityImageCubeArray); break;
      case DxbcResourceDim::TextureCubeArr: m_module.enableCapability(spv::CapabilityImageCubeArray); break;
      case DxbcResourceDim::Texture2DMsArr: m_module.enableCapability(spv::CapabilityImageMSArray);   break;
      default: break; // No additional capabilities required
    }
    
    // We do not know whether the image is going to be used as
    // a color image or a depth image yet, but we can pick the
    // correct type when creating a sampled image object.
    const uint32_t imageTypeId = m_module.defImageType(sampledTypeId,
      typeInfo.dim, 0, typeInfo.array, typeInfo.ms, typeInfo.sampled,
      spv::ImageFormatUnknown);
    
    // We'll declare the texture variable with the color type
    // and decide which one to use when the texture is sampled.
    const uint32_t resourcePtrType = m_module.defPointerType(
      imageTypeId, spv::StorageClassUniformConstant);
    
    const uint32_t varId = m_module.newVar(resourcePtrType,
      spv::StorageClassUniformConstant);
    
    m_module.setDebugName(varId,
      str::format(isUav ? "u" : "t", registerId).c_str());
    
    // Compute the DXVK binding slot index for the resource.
    // D3D11 needs to bind the actual resource to this slot.
    const uint32_t bindingId = computeResourceSlotId(
      m_version.type(), isUav
        ? DxbcBindingType::UnorderedAccessView
        : DxbcBindingType::ShaderResource,
      registerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Declare a specialization constant which will
    // store whether or not the resource is bound.
    const uint32_t specConstId = m_module.specConstBool(true);
    m_module.decorateSpecId(specConstId, bindingId);
    m_module.setDebugName(specConstId,
      str::format(isUav ? "u" : "t", registerId, "_bound").c_str());
    
    if (isUav) {
      DxbcUav uav;
      uav.type          = DxbcResourceType::Typed;
      uav.imageInfo     = typeInfo;
      uav.varId         = varId;
      uav.ctrId         = 0;
      uav.specId        = specConstId;
      uav.sampledType   = sampledType;
      uav.sampledTypeId = sampledTypeId;
      uav.imageTypeId   = imageTypeId;
      uav.structStride  = 0;
      m_uavs.at(registerId) = uav;
    } else {
      DxbcShaderResource res;
      res.type          = DxbcResourceType::Typed;
      res.imageInfo     = typeInfo;
      res.varId         = varId;
      res.specId        = specConstId;
      res.sampledType   = sampledType;
      res.sampledTypeId = sampledTypeId;
      res.imageTypeId   = imageTypeId;
      res.colorTypeId   = imageTypeId;
      res.depthTypeId   = 0;
      res.structStride  = 0;
      
      if (resourceType == DxbcResourceDim::Texture2D
       || resourceType == DxbcResourceDim::Texture2DArr
       || resourceType == DxbcResourceDim::TextureCube
       || resourceType == DxbcResourceDim::TextureCubeArr) {
        res.depthTypeId = m_module.defImageType(sampledTypeId,
          typeInfo.dim, 1, typeInfo.array, typeInfo.ms, typeInfo.sampled,
          spv::ImageFormatUnknown);
      }
      
      m_textures.at(registerId) = res;
    }
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.view = getViewType(resourceType);
    
    if (isUav) {
      resource.type = resourceType == DxbcResourceDim::Buffer
        ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
        : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    } else {
      resource.type = resourceType == DxbcResourceDim::Buffer
        ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
        : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    
    m_resourceSlots.push_back(resource);
  }
  
  
  void DxbcCompiler::emitDclResourceRawStructured(const DxbcShaderInstruction& ins) {
    // dcl_resource_raw and dcl_uav_raw take one argument:
    //    (dst0) The resource register ID
    // dcl_resource_structured and dcl_uav_structured take two arguments:
    //    (dst0) The resource register ID
    //    (imm0) Structure stride, in bytes
    const uint32_t registerId = ins.dst[0].idx[0].offset;
    
    const bool isUav = ins.op == DxbcOpcode::DclUavRaw
                    || ins.op == DxbcOpcode::DclUavStructured;
    
    const bool isStructured = ins.op == DxbcOpcode::DclUavStructured
                           || ins.op == DxbcOpcode::DclResourceStructured;
    
    // Structured and raw buffers are represented as
    // texel buffers consisting of 32-bit integers.
    m_module.enableCapability(spv::CapabilityImageBuffer);
    
    const DxbcScalarType sampledType = DxbcScalarType::Uint32;
    const uint32_t sampledTypeId = getScalarTypeId(sampledType);
    
    const DxbcImageInfo typeInfo = { spv::DimBuffer, 0, 0, isUav ? 2u : 1u };
    
    // Declare the resource type
    const uint32_t resTypeId = m_module.defImageType(sampledTypeId,
      typeInfo.dim, 0, typeInfo.array, typeInfo.ms, typeInfo.sampled,
      spv::ImageFormatR32ui);
    
    const uint32_t varId = m_module.newVar(
      m_module.defPointerType(resTypeId, spv::StorageClassUniformConstant),
      spv::StorageClassUniformConstant);
    
    m_module.setDebugName(varId,
      str::format(isUav ? "u" : "t", registerId).c_str());
    
    // Write back resource info
    const DxbcResourceType resType = isStructured
      ? DxbcResourceType::Structured
      : DxbcResourceType::Raw;
    
    const uint32_t resStride = isStructured
      ? ins.imm[0].u32
      : 0;
    
    // Compute the DXVK binding slot index for the resource.
    const uint32_t bindingId = computeResourceSlotId(
      m_version.type(), isUav
        ? DxbcBindingType::UnorderedAccessView
        : DxbcBindingType::ShaderResource,
      registerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Declare a specialization constant which will
    // store whether or not the resource is bound.
    const uint32_t specConstId = m_module.specConstBool(true);
    m_module.decorateSpecId(specConstId, bindingId);
    m_module.setDebugName(specConstId,
      str::format(isUav ? "u" : "t", registerId, "_bound").c_str());
    
    if (isUav) {
      DxbcUav uav;
      uav.type          = resType;
      uav.imageInfo     = typeInfo;
      uav.varId         = varId;
      uav.ctrId         = 0;
      uav.specId        = specConstId;
      uav.sampledType   = sampledType;
      uav.sampledTypeId = sampledTypeId;
      uav.imageTypeId   = resTypeId;
      uav.structStride  = resStride;
      m_uavs.at(registerId) = uav;
    } else {
      DxbcShaderResource res;
      res.type          = resType;
      res.imageInfo     = typeInfo;
      res.varId         = varId;
      res.specId        = specConstId;
      res.sampledType   = sampledType;
      res.sampledTypeId = sampledTypeId;
      res.imageTypeId   = resTypeId;
      res.colorTypeId   = resTypeId;
      res.depthTypeId   = 0;
      res.structStride  = resStride;
      m_textures.at(registerId) = res;
    }
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = isUav
      ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
      : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    resource.view = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    m_resourceSlots.push_back(resource);
  }
  
  
  void DxbcCompiler::emitDclThreadGroupSharedMemory(const DxbcShaderInstruction& ins) {
    // dcl_tgsm_raw takes two arguments:
    //    (dst0) The resource register ID
    //    (imm0) Block size, in DWORDs
    // dcl_tgsm_structured takes three arguments:
    //    (dst0) The resource register ID
    //    (imm0) Structure stride, in bytes
    //    (imm1) Structure count
    const bool isStructured = ins.op == DxbcOpcode::DclThreadGroupSharedMemoryStructured;
    
    const uint32_t regId = ins.dst[0].idx[0].offset;
    
    if (regId >= m_gRegs.size())
      m_gRegs.resize(regId + 1);
    
    const uint32_t elementStride = isStructured ? ins.imm[0].u32 : 0;
    const uint32_t elementCount  = isStructured ? ins.imm[1].u32 : ins.imm[0].u32;
    
    DxbcRegisterInfo varInfo;
    varInfo.type.ctype   = DxbcScalarType::Uint32;
    varInfo.type.ccount  = 1;
    varInfo.type.alength = isStructured
      ? elementCount * elementStride / 4
      : elementCount;
    varInfo.sclass = spv::StorageClassWorkgroup;
    
    m_gRegs[regId].type = isStructured
      ? DxbcResourceType::Structured
      : DxbcResourceType::Raw;
    m_gRegs[regId].elementStride = elementStride;
    m_gRegs[regId].elementCount  = elementCount;
    m_gRegs[regId].varId = emitNewVariable(varInfo);
    
    m_module.setDebugName(m_gRegs[regId].varId,
      str::format("g", regId).c_str());
  }
  
  
  void DxbcCompiler::emitDclGsInputPrimitive(const DxbcShaderInstruction& ins) {
    // The input primitive type is stored within in the
    // control bits of the opcode token. In SPIR-V, we
    // have to define an execution mode.
    const spv::ExecutionMode mode = [&] {
      switch (ins.controls.primitive) {
        case DxbcPrimitive::Point:       return spv::ExecutionModeInputPoints;
        case DxbcPrimitive::Line:        return spv::ExecutionModeInputLines;
        case DxbcPrimitive::Triangle:    return spv::ExecutionModeTriangles;
        case DxbcPrimitive::LineAdj:     return spv::ExecutionModeInputLinesAdjacency;
        case DxbcPrimitive::TriangleAdj: return spv::ExecutionModeInputTrianglesAdjacency;
        default: throw DxvkError("DxbcCompiler: Unsupported primitive type");
      }
    }();
    
    m_gs.inputPrimitive = ins.controls.primitive;
    m_module.setExecutionMode(m_entryPointId, mode);
    
    const uint32_t vertexCount
      = primitiveVertexCount(m_gs.inputPrimitive);
    
    emitDclInputArray(vertexCount);
    emitDclInputPerVertex(vertexCount, "gs_vertex_in");
  }
  
  
  void DxbcCompiler::emitDclGsOutputTopology(const DxbcShaderInstruction& ins) {
    // The input primitive topology is stored within in the
    // control bits of the opcode token. In SPIR-V, we have
    // to define an execution mode.
    const spv::ExecutionMode mode = [&] {
      switch (ins.controls.primitiveTopology) {
        case DxbcPrimitiveTopology::PointList:     return spv::ExecutionModeOutputPoints;
        case DxbcPrimitiveTopology::LineStrip:     return spv::ExecutionModeOutputLineStrip;
        case DxbcPrimitiveTopology::TriangleStrip: return spv::ExecutionModeOutputTriangleStrip;
        default: throw DxvkError("DxbcCompiler: Unsupported primitive topology");
      }
    }();
    
    m_module.setExecutionMode(m_entryPointId, mode);
  }
  
  
  void DxbcCompiler::emitDclMaxOutputVertexCount(const DxbcShaderInstruction& ins) {
    // dcl_max_output_vertex_count has one operand:
    //    (imm0) The maximum number of vertices
    m_gs.outputVertexCount = ins.imm[0].u32;
    m_module.setOutputVertices(m_entryPointId, m_gs.outputVertexCount);
  }
  
  
  void DxbcCompiler::emitDclThreadGroup(const DxbcShaderInstruction& ins) {
    // dcl_thread_group has three operands:
    //    (imm0) Number of threads in X dimension
    //    (imm1) Number of threads in Y dimension
    //    (imm2) Number of threads in Z dimension
    m_module.setLocalSize(m_entryPointId,
      ins.imm[0].u32, ins.imm[1].u32, ins.imm[2].u32);
  }
  
  
  uint32_t DxbcCompiler::emitDclUavCounter(uint32_t regId) {
    // Declare a structure type which holds the UAV counter
    if (m_uavCtrStructType == 0) {
      const uint32_t t_u32    = m_module.defIntType(32, 0);
      const uint32_t t_struct = m_module.defStructTypeUnique(1, &t_u32);
      
      m_module.decorate(t_struct, spv::DecorationBufferBlock);
      m_module.memberDecorateOffset(t_struct, 0, 0);
      
      m_module.setDebugName      (t_struct, "uav_meta");
      m_module.setDebugMemberName(t_struct, 0, "ctr");
      
      m_uavCtrStructType  = t_struct;
      m_uavCtrPointerType = m_module.defPointerType(
        t_struct, spv::StorageClassUniform);
    }
    
    // Declare the buffer variable
    const uint32_t varId = m_module.newVar(
      m_uavCtrPointerType, spv::StorageClassUniform);
    
    m_module.setDebugName(varId,
      str::format("u", regId, "_meta").c_str());
    
    const uint32_t bindingId = computeResourceSlotId(
      m_version.type(), DxbcBindingType::UavCounter,
      regId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Declare the storage buffer binding
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    resource.view = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    m_resourceSlots.push_back(resource);
    
    return varId;
  }
  
  
  void DxbcCompiler::emitDclImmediateConstantBuffer(const DxbcShaderInstruction& ins) {
    if (m_immConstBuf != 0)
      throw DxvkError("DxbcCompiler: Immediate constant buffer already declared");
    
    if ((ins.customDataSize & 0x3) != 0)
      throw DxvkError("DxbcCompiler: Immediate constant buffer size not a multiple of four DWORDs");
    
    // Declare individual vector constants as 4x32-bit vectors
    std::array<uint32_t, 4096> vectorIds;
    
    DxbcVectorType vecType;
    vecType.ctype  = DxbcScalarType::Uint32;
    vecType.ccount = 4;
    
    const uint32_t vectorTypeId = getVectorTypeId(vecType);
    const uint32_t vectorCount  = ins.customDataSize / 4;
    
    for (uint32_t i = 0; i < vectorCount; i++) {
      std::array<uint32_t, 4> scalarIds = {
        m_module.constu32(ins.customData[4 * i + 0]),
        m_module.constu32(ins.customData[4 * i + 1]),
        m_module.constu32(ins.customData[4 * i + 2]),
        m_module.constu32(ins.customData[4 * i + 3]),
      };
      
      vectorIds.at(i) = m_module.constComposite(
        vectorTypeId, scalarIds.size(), scalarIds.data());
    }
    
    // Declare the array that contains all the vectors
    DxbcArrayType arrInfo;
    arrInfo.ctype   = DxbcScalarType::Uint32;
    arrInfo.ccount  = 4;
    arrInfo.alength = vectorCount;
    
    const uint32_t arrayTypeId = getArrayTypeId(arrInfo);
    const uint32_t arrayId = m_module.constComposite(
      arrayTypeId, vectorCount, vectorIds.data());
    
    // Declare the variable that will hold the constant
    // data and initialize it with the constant array.
    const uint32_t pointerTypeId = m_module.defPointerType(
      arrayTypeId, spv::StorageClassPrivate);
    
    m_immConstBuf = m_module.newVarInit(
      pointerTypeId, spv::StorageClassPrivate,
      arrayId);
    m_module.setDebugName(m_immConstBuf, "icb");
  }
  
  
  void DxbcCompiler::emitCustomData(const DxbcShaderInstruction& ins) {
    switch (ins.customDataType) {
      case DxbcCustomDataClass::ImmConstBuf:
        return emitDclImmediateConstantBuffer(ins);
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unsupported custom data block: ",
          ins.customDataType));
    }
  }
  
  
  void DxbcCompiler::emitVectorAlu(const DxbcShaderInstruction& ins) {
    std::array<DxbcRegisterValue, DxbcMaxOperandCount> src;
    
    for (uint32_t i = 0; i < ins.srcCount; i++)
      src.at(i) = emitRegisterLoad(ins.src[i], ins.dst[0].mask);
    
    DxbcRegisterValue dst;
    dst.type.ctype  = ins.dst[0].dataType;
    dst.type.ccount = ins.dst[0].mask.setCount();
    
    const uint32_t typeId = getVectorTypeId(dst.type);
    
    switch (ins.op) {
      /////////////////////
      // Move instructions
      case DxbcOpcode::Mov:
        dst.id = src.at(0).id;
        break;
        
      /////////////////////////////////////
      // ALU operations on float32 numbers
      case DxbcOpcode::Add:
        dst.id = m_module.opFAdd(typeId,
          src.at(0).id, src.at(1).id);
        break;
        
      case DxbcOpcode::Div:
        dst.id = m_module.opFDiv(typeId,
          src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Exp:
        dst.id = m_module.opExp2(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::Frc:
        dst.id = m_module.opFract(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::Log:
        dst.id = m_module.opLog2(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::Mad:
        dst.id = m_module.opFFma(typeId,
          src.at(0).id, src.at(1).id, src.at(2).id);
        break;
      
      case DxbcOpcode::Max:
        dst.id = m_options.useSimpleMinMaxClamp
          ? m_module.opFMax(typeId, src.at(0).id, src.at(1).id)
          : m_module.opNMax(typeId, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Min:
        dst.id = m_options.useSimpleMinMaxClamp
          ? m_module.opFMin(typeId, src.at(0).id, src.at(1).id)
          : m_module.opNMin(typeId, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Mul:
        dst.id = m_module.opFMul(typeId,
          src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Rcp: {
        dst.id = m_module.opFDiv(typeId,
          emitBuildConstVecf32(
            1.0f, 1.0f, 1.0f, 1.0f,
            ins.dst[0].mask).id,
          src.at(0).id);
      } break;
      
      case DxbcOpcode::RoundNe:
        dst.id = m_module.opRoundEven(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::RoundNi:
        dst.id = m_module.opFloor(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::RoundPi:
        dst.id = m_module.opCeil(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::RoundZ:
        dst.id = m_module.opTrunc(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::Rsq:
        dst.id = m_module.opInverseSqrt(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::Sqrt:
        dst.id = m_module.opSqrt(
          typeId, src.at(0).id);
        break;
      
      /////////////////////////////////////
      // ALU operations on signed integers
      case DxbcOpcode::IAdd:
        dst.id = m_module.opIAdd(typeId,
          src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::IMad:
      case DxbcOpcode::UMad:
        dst.id = m_module.opIAdd(typeId,
          m_module.opIMul(typeId,
            src.at(0).id, src.at(1).id),
          src.at(2).id);
        break;
      
      case DxbcOpcode::IMax:
        dst.id = m_module.opSMax(typeId,
          src.at(0).id, src.at(1).id);
        break;
        
      case DxbcOpcode::IMin:
        dst.id = m_module.opSMin(typeId,
          src.at(0).id, src.at(1).id);
        break;
        
      case DxbcOpcode::INeg:
        dst.id = m_module.opSNegate(
          typeId, src.at(0).id);
        break;
      
      ///////////////////////////////////////
      // ALU operations on unsigned integers
      case DxbcOpcode::UMax:
        dst.id = m_module.opUMax(typeId,
          src.at(0).id, src.at(1).id);
        break;
        
      case DxbcOpcode::UMin:
        dst.id = m_module.opUMin(typeId,
          src.at(0).id, src.at(1).id);
        break;
      
      ///////////////////////////////////////
      // Bit operations on unsigned integers
      case DxbcOpcode::And:
        dst.id = m_module.opBitwiseAnd(typeId,
          src.at(0).id, src.at(1).id);
        break;
        
      case DxbcOpcode::Not:
        dst.id = m_module.opNot(
          typeId, src.at(0).id);
        break;
        
      case DxbcOpcode::Or:
        dst.id = m_module.opBitwiseOr(typeId,
          src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Xor:
        dst.id = m_module.opBitwiseXor(typeId,
          src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::CountBits:
        dst.id = m_module.opBitCount(
          typeId, src.at(0).id);
        break;
        
      case DxbcOpcode::FirstBitLo:
        dst.id = m_module.opFindILsb(
          typeId, src.at(0).id);
        break;
        
      case DxbcOpcode::FirstBitHi:
        dst.id = m_module.opFindUMsb(
          typeId, src.at(0).id);
        break;
        
      case DxbcOpcode::FirstBitShi:
        dst.id = m_module.opFindSMsb(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::BfRev:
        dst.id = m_module.opBitReverse(
          typeId, src.at(0).id);
        break;
      
      ///////////////////////////
      // Conversion instructions
      case DxbcOpcode::ItoF:
        dst.id = m_module.opConvertStoF(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::UtoF:
        dst.id = m_module.opConvertUtoF(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::FtoI:
        dst.id = m_module.opConvertFtoS(
          typeId, src.at(0).id);
        break;
      
      case DxbcOpcode::FtoU:
        dst.id = m_module.opConvertFtoU(
          typeId, src.at(0).id);
        break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    // Store computed value
    dst = emitDstOperandModifiers(dst, ins.modifiers);
    emitRegisterStore(ins.dst[0], dst);
  }
  
  
  void DxbcCompiler::emitVectorCmov(const DxbcShaderInstruction& ins) {
    // movc and swapc have the following operands:
    //    (dst0) The first destination register
    //    (dst1) The second destination register (swapc only)
    //    (src0) The condition vector
    //    (src1) Vector to select from if the condition is not 0
    //    (src2) Vector to select from if the condition is 0
    const DxbcRegisterValue condition   = emitRegisterLoad(ins.src[0], ins.dst[0].mask);
    const DxbcRegisterValue selectTrue  = emitRegisterLoad(ins.src[1], ins.dst[0].mask);
    const DxbcRegisterValue selectFalse = emitRegisterLoad(ins.src[2], ins.dst[0].mask);
    
    const uint32_t componentCount = ins.dst[0].mask.setCount();
    
    // We'll compare against a vector of zeroes to generate a
    // boolean vector, which in turn will be used by OpSelect
    uint32_t zeroType = m_module.defIntType(32, 0);
    uint32_t boolType = m_module.defBoolType();
    
    uint32_t zero = m_module.constu32(0);
    
    if (componentCount > 1) {
      zeroType = m_module.defVectorType(zeroType, componentCount);
      boolType = m_module.defVectorType(boolType, componentCount);
      
      const std::array<uint32_t, 4> zeroVec = { zero, zero, zero, zero };
      zero = m_module.constComposite(zeroType, componentCount, zeroVec.data());
    }
    
    // In case of swapc, the second destination operand receives
    // the output that a cmov instruction would normally get
    const uint32_t trueIndex = ins.op == DxbcOpcode::Swapc ? 1 : 0;
    
    for (uint32_t i = 0; i < ins.dstCount; i++) {
      DxbcRegisterValue result;
      result.type.ctype  = ins.dst[i].dataType;
      result.type.ccount = componentCount;
      result.id = m_module.opSelect(
        getVectorTypeId(result.type),
        m_module.opINotEqual(boolType, condition.id, zero),
        i == trueIndex ? selectTrue.id : selectFalse.id,
        i != trueIndex ? selectTrue.id : selectFalse.id);
      
      result = emitDstOperandModifiers(result, ins.modifiers);
      emitRegisterStore(ins.dst[i], result);
    }
  }
  
  void DxbcCompiler::emitVectorCmp(const DxbcShaderInstruction& ins) {
    // Compare instructions have three operands:
    //    (dst0) The destination register
    //    (src0) The first vector to compare
    //    (src1) The second vector to compare
    const std::array<DxbcRegisterValue, 2> src = {
      emitRegisterLoad(ins.src[0], ins.dst[0].mask),
      emitRegisterLoad(ins.src[1], ins.dst[0].mask),
    };
    
    const uint32_t componentCount = ins.dst[0].mask.setCount();
    
    // Condition, which is a boolean vector used
    // to select between the ~0u and 0u vectors.
    uint32_t condition     = 0;
    uint32_t conditionType = m_module.defBoolType();
    
    if (componentCount > 1)
      conditionType = m_module.defVectorType(conditionType, componentCount);
    
    switch (ins.op) {
      case DxbcOpcode::Eq:
        condition = m_module.opFOrdEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Ge:
        condition = m_module.opFOrdGreaterThanEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Lt:
        condition = m_module.opFOrdLessThan(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::Ne:
        condition = m_module.opFOrdNotEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::IEq:
        condition = m_module.opIEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::IGe:
        condition = m_module.opSGreaterThanEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::ILt:
        condition = m_module.opSLessThan(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::INe:
        condition = m_module.opINotEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::UGe:
        condition = m_module.opUGreaterThanEqual(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      case DxbcOpcode::ULt:
        condition = m_module.opULessThan(
          conditionType, src.at(0).id, src.at(1).id);
        break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    // Generate constant vectors for selection
    uint32_t sFalse = m_module.constu32( 0u);
    uint32_t sTrue  = m_module.constu32(~0u);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = componentCount;
    
    const uint32_t typeId = getVectorTypeId(result.type);
    
    if (componentCount > 1) {
      const std::array<uint32_t, 4> vFalse = { sFalse, sFalse, sFalse, sFalse };
      const std::array<uint32_t, 4> vTrue  = { sTrue,  sTrue,  sTrue,  sTrue  };
      
      sFalse = m_module.constComposite(typeId, componentCount, vFalse.data());
      sTrue  = m_module.constComposite(typeId, componentCount, vTrue .data());
    }
    
    // Perform component-wise mask selection
    // based on the condition evaluated above.
    result.id = m_module.opSelect(
      typeId, condition, sTrue, sFalse);
    
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitVectorDeriv(const DxbcShaderInstruction& ins) {
    // Derivative instructions have two operands:
    //    (dst0) Destination register for the derivative
    //    (src0) The operand to compute the derivative of
    DxbcRegisterValue value = emitRegisterLoad(ins.src[0], ins.dst[0].mask);
    const uint32_t typeId = getVectorTypeId(value.type);
    
    switch (ins.op) {
      case DxbcOpcode::DerivRtx:
        value.id = m_module.opDpdx(typeId, value.id);
        break;
        
      case DxbcOpcode::DerivRty:
        value.id = m_module.opDpdy(typeId, value.id);
        break;
        
      case DxbcOpcode::DerivRtxCoarse:
        value.id = m_module.opDpdxCoarse(typeId, value.id);
        break;
        
      case DxbcOpcode::DerivRtyCoarse:
        value.id = m_module.opDpdyCoarse(typeId, value.id);
        break;
        
      case DxbcOpcode::DerivRtxFine:
        value.id = m_module.opDpdxFine(typeId, value.id);
        break;
        
      case DxbcOpcode::DerivRtyFine:
        value.id = m_module.opDpdyFine(typeId, value.id);
        break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    value = emitDstOperandModifiers(value, ins.modifiers);
    emitRegisterStore(ins.dst[0], value);
  }
  
  
  void DxbcCompiler::emitVectorDot(const DxbcShaderInstruction& ins) {
    const DxbcRegMask srcMask(true,
      ins.op >= DxbcOpcode::Dp2,
      ins.op >= DxbcOpcode::Dp3,
      ins.op >= DxbcOpcode::Dp4);
    
    const std::array<DxbcRegisterValue, 2> src = {
      emitRegisterLoad(ins.src[0], srcMask),
      emitRegisterLoad(ins.src[1], srcMask),
    };
    
    DxbcRegisterValue dst;
    dst.type.ctype  = ins.dst[0].dataType;
    dst.type.ccount = 1;
    
    dst.id = m_module.opDot(
      getVectorTypeId(dst.type),
      src.at(0).id,
      src.at(1).id);
    
    dst = emitDstOperandModifiers(dst, ins.modifiers);
    emitRegisterStore(ins.dst[0], dst);
  }
  
  
  void DxbcCompiler::emitVectorIdiv(const DxbcShaderInstruction& ins) {
    // udiv has four operands:
    //    (dst0) Quotient destination register
    //    (dst1) Remainder destination register
    //    (src0) The first vector to compare
    //    (src1) The second vector to compare
    if (ins.dst[0].type == DxbcOperandType::Null
     && ins.dst[1].type == DxbcOperandType::Null)
      return;
    
    // FIXME support this if applications require it
    if (ins.dst[0].type != DxbcOperandType::Null
     && ins.dst[1].type != DxbcOperandType::Null
     && ins.dst[0].mask != ins.dst[1].mask) {
      Logger::warn("DxbcCompiler: Idiv with different destination masks not supported");
      return;
    }
    
    // Load source operands as integers with the
    // mask of one non-NULL destination operand
    const DxbcRegMask srcMask =
      ins.dst[0].type != DxbcOperandType::Null
        ? ins.dst[0].mask
        : ins.dst[1].mask;
    
    const std::array<DxbcRegisterValue, 2> src = {
      emitRegisterLoad(ins.src[0], srcMask),
      emitRegisterLoad(ins.src[1], srcMask),
    };
    
    // Compute results only if the destination
    // operands are not NULL.
    if (ins.dst[0].type != DxbcOperandType::Null) {
      DxbcRegisterValue quotient;
      quotient.type.ctype  = ins.dst[0].dataType;
      quotient.type.ccount = ins.dst[0].mask.setCount();
      
      quotient.id = m_module.opUDiv(
        getVectorTypeId(quotient.type),
        src.at(0).id, src.at(1).id);
      
      quotient = emitDstOperandModifiers(quotient, ins.modifiers);
      emitRegisterStore(ins.dst[0], quotient);
    }
    
    if (ins.dst[1].type != DxbcOperandType::Null) {
      DxbcRegisterValue remainder;
      remainder.type.ctype  = ins.dst[1].dataType;
      remainder.type.ccount = ins.dst[1].mask.setCount();
      
      remainder.id = m_module.opUMod(
        getVectorTypeId(remainder.type),
        src.at(0).id, src.at(1).id);
      
      remainder = emitDstOperandModifiers(remainder, ins.modifiers);
      emitRegisterStore(ins.dst[1], remainder);
    }
  }
  
  
  void DxbcCompiler::emitVectorImul(const DxbcShaderInstruction& ins) {
    // imul and umul have four operands:
    //    (dst0) High destination register
    //    (dst1) Low destination register
    //    (src0) The first vector to compare
    //    (src1) The second vector to compare
    if (ins.dst[0].type == DxbcOperandType::Null) {
      if (ins.dst[1].type == DxbcOperandType::Null)
        return;
      
      // If dst0 is NULL, this instruction behaves just
      // like any other three-operand ALU instruction
      const std::array<DxbcRegisterValue, 2> src = {
        emitRegisterLoad(ins.src[0], ins.dst[1].mask),
        emitRegisterLoad(ins.src[1], ins.dst[1].mask),
      };
      
      DxbcRegisterValue result;
      result.type.ctype  = ins.dst[1].dataType;
      result.type.ccount = ins.dst[1].mask.setCount();
      result.id = m_module.opIMul(
        getVectorTypeId(result.type),
        src.at(0).id, src.at(1).id);
      
      result = emitDstOperandModifiers(result, ins.modifiers);
      emitRegisterStore(ins.dst[1], result);
    } else {
      // TODO implement this
      Logger::warn("DxbcCompiler: Extended Imul not yet supported");
    }
  }
  
  
  void DxbcCompiler::emitVectorShift(const DxbcShaderInstruction& ins) {
    // Shift operations have three operands:
    //    (dst0) The destination register
    //    (src0) The register to shift
    //    (src1) The shift amount (scalar)
    DxbcRegisterValue shiftReg = emitRegisterLoad(ins.src[0], ins.dst[0].mask);
    DxbcRegisterValue countReg = emitRegisterLoad(ins.src[1], ins.dst[0].mask);
    
    if (countReg.type.ccount == 1)
      countReg = emitRegisterExtend(countReg, shiftReg.type.ccount);
    
    DxbcRegisterValue result;
    result.type.ctype  = ins.dst[0].dataType;
    result.type.ccount = ins.dst[0].mask.setCount();
    
    switch (ins.op) {
      case DxbcOpcode::IShl:
        result.id = m_module.opShiftLeftLogical(
          getVectorTypeId(result.type),
          shiftReg.id, countReg.id);
        break;
      
      case DxbcOpcode::IShr:
        result.id = m_module.opShiftRightArithmetic(
          getVectorTypeId(result.type),
          shiftReg.id, countReg.id);
        break;
      
      case DxbcOpcode::UShr:
        result.id = m_module.opShiftRightLogical(
          getVectorTypeId(result.type),
          shiftReg.id, countReg.id);
        break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    result = emitDstOperandModifiers(result, ins.modifiers);
    emitRegisterStore(ins.dst[0], result);
  }
  
    
  void DxbcCompiler::emitVectorSinCos(const DxbcShaderInstruction& ins) {
    // sincos has three operands:
    //    (dst0) Destination register for sin(x)
    //    (dst1) Destination register for cos(x)
    //    (src0) Source operand x
    
    // Load source operand as 32-bit float vector.
    const DxbcRegisterValue srcValue = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, true, true, true));
    
    // Either output may be DxbcOperandType::Null, in
    // which case we don't have to generate any code.
    if (ins.dst[0].type != DxbcOperandType::Null) {
      const DxbcRegisterValue sinInput =
        emitRegisterExtract(srcValue, ins.dst[0].mask);
      
      DxbcRegisterValue sin;
      sin.type = sinInput.type;
      sin.id = m_module.opSin(
        getVectorTypeId(sin.type),
        sinInput.id);
      
      emitRegisterStore(ins.dst[0], sin);
    }
    
    if (ins.dst[1].type != DxbcOperandType::Null) {
      const DxbcRegisterValue cosInput =
        emitRegisterExtract(srcValue, ins.dst[1].mask);
      
      DxbcRegisterValue cos;
      cos.type = cosInput.type;
      cos.id = m_module.opCos(
        getVectorTypeId(cos.type),
        cosInput.id);
      
      emitRegisterStore(ins.dst[1], cos);
    }
  }
  
  
  void DxbcCompiler::emitGeometryEmit(const DxbcShaderInstruction& ins) {
    switch (ins.op) {
      case DxbcOpcode::Emit:
      case DxbcOpcode::EmitStream: {
        emitOutputSetup();
        m_module.opEmitVertex();
      } break;
      
      case DxbcOpcode::Cut:
      case DxbcOpcode::CutStream: {
        m_module.opEndPrimitive();
      } break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
    }
  }
  
  
  void DxbcCompiler::emitAtomic(const DxbcShaderInstruction& ins) {
    // atomic_* operations have the following operands:
    //    (dst0) Destination u# or g# register
    //    (src0) Index into the texture or buffer
    //    (src1) The source value for the operation
    //    (src2) Second source operand (optional)
    // imm_atomic_* operations have the following operands:
    //    (dst0) Register that receives the result
    //    (dst1) Destination u# or g# register
    //    (srcX) As above
    const bool isImm = ins.dstCount == 2;
    const bool isUav = ins.dst[ins.dstCount - 1].type == DxbcOperandType::UnorderedAccessView;
    
    // Retrieve destination pointer for the atomic operation
    const DxbcRegisterPointer pointer = emitGetAtomicPointer(
      ins.dst[ins.dstCount - 1], ins.src[0]);
    
    // Load source values
    std::array<DxbcRegisterValue, 2> src;
    
    for (uint32_t i = 1; i < ins.srcCount; i++) {
      src[i - 1] = emitRegisterLoad(ins.src[i],
        DxbcRegMask(true, false, false, false));
    }
    
    // Define memory scope and semantics based on the operands
    uint32_t semantics = isUav
      ? spv::MemorySemanticsUniformMemoryMask
      : spv::MemorySemanticsWorkgroupMemoryMask;
    
    // TODO verify whether this is even remotely correct
    semantics |= spv::MemorySemanticsAcquireReleaseMask;
    
    // TODO for UAVs, respect globally coherent flag
    const uint32_t scopeId     = m_module.constu32(spv::ScopeWorkgroup);
    const uint32_t semanticsId = m_module.constu32(semantics);
    
    // Perform the atomic operation on the given pointer
    DxbcRegisterValue value;
    value.type.ctype  = ins.dst[0].dataType;
    value.type.ccount = 1;
    value.id = 0;
    
    // The result type, which is a scalar integer
    const uint32_t typeId = getVectorTypeId(value.type);
    
    // TODO add signed min/max
    switch (ins.op) {
      case DxbcOpcode::ImmAtomicExch:
        value.id = m_module.opAtomicExchange(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicIAdd:
      case DxbcOpcode::ImmAtomicIAdd:
        value.id = m_module.opAtomicIAdd(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicAnd:
      case DxbcOpcode::ImmAtomicAnd:
        value.id = m_module.opAtomicAnd(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicOr:
      case DxbcOpcode::ImmAtomicOr:
        value.id = m_module.opAtomicOr(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicXor:
      case DxbcOpcode::ImmAtomicXor:
        value.id = m_module.opAtomicXor(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicIMin:
        value.id = m_module.opAtomicSMin(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicIMax:
        value.id = m_module.opAtomicSMax(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicUMin:
        value.id = m_module.opAtomicUMin(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      case DxbcOpcode::AtomicUMax:
        value.id = m_module.opAtomicUMax(typeId,
          pointer.id, scopeId, semanticsId,
          src[0].id);
        break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    // Write back the result to the destination
    // register if this is an imm_atomic_* opcode.
    if (isImm)
      emitRegisterStore(ins.dst[0], value);
  }
  
  
  void DxbcCompiler::emitAtomicCounter(const DxbcShaderInstruction& ins) {
    // imm_atomic_alloc and imm_atomic_consume have the following operands:
    //    (dst0) The register that will hold the old counter value
    //    (dst1) The UAV whose counter is going to be modified
    // TODO check if the corresponding UAV is bound
    const uint32_t registerId = ins.dst[1].idx[0].offset;
    
    if (m_uavs.at(registerId).ctrId == 0)
      m_uavs.at(registerId).ctrId = emitDclUavCounter(registerId);
    
    // Get a pointer to the atomic counter in question
    DxbcRegisterInfo ptrType;
    ptrType.type.ctype   = DxbcScalarType::Uint32;
    ptrType.type.ccount  = 1;
    ptrType.type.alength = 0;
    ptrType.sclass = spv::StorageClassUniform;
    
    const uint32_t zeroId = m_module.consti32(0);
    const uint32_t ptrId  = m_module.opAccessChain(
      getPointerTypeId(ptrType),
      m_uavs.at(registerId).ctrId,
      1, &zeroId);
    
    // Define memory scope and semantics based on the operands
    uint32_t scope     = spv::ScopeDevice;
    uint32_t semantics = spv::MemorySemanticsUniformMemoryMask
                       | spv::MemorySemanticsAcquireReleaseMask;
    
    const uint32_t scopeId     = m_module.constu32(scope);
    const uint32_t semanticsId = m_module.constu32(semantics);
    
    // Compute the result value
    DxbcRegisterValue value;
    value.type.ctype  = DxbcScalarType::Uint32;
    value.type.ccount = 1;
    
    const uint32_t typeId = getVectorTypeId(value.type);
    
    switch (ins.op) {
      case DxbcOpcode::ImmAtomicAlloc:
        value.id = m_module.opAtomicIAdd(typeId, ptrId,
          scopeId, semanticsId, m_module.constu32(1));
        break;
        
      case DxbcOpcode::ImmAtomicConsume:
        value.id = m_module.opAtomicISub(typeId, ptrId,
          scopeId, semanticsId, m_module.constu32(1));
        break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    // Store the result
    emitRegisterStore(ins.dst[0], value);
  }
  
  
  void DxbcCompiler::emitBarrier(const DxbcShaderInstruction& ins) {
    // sync takes no operands. Instead, the synchronization
    // scope is defined by the operand control bits.
    const DxbcSyncFlags flags = ins.controls.syncFlags;
    
    uint32_t executionScope   = 0;
    uint32_t memoryScope      = 0;
    uint32_t memorySemantics  = 0;
    
    if (flags.test(DxbcSyncFlag::ThreadsInGroup))
      executionScope   = spv::ScopeWorkgroup;
    
    if (flags.test(DxbcSyncFlag::ThreadGroupSharedMemory)) {
      memoryScope      = spv::ScopeWorkgroup;
      memorySemantics |= spv::MemorySemanticsWorkgroupMemoryMask;
    }
    
    if (flags.test(DxbcSyncFlag::UavMemoryGroup)) {
      memoryScope      = spv::ScopeWorkgroup;
      memorySemantics |= spv::MemorySemanticsUniformMemoryMask;
    }
    
    if (flags.test(DxbcSyncFlag::UavMemoryGlobal)) {
      memoryScope      = spv::ScopeDevice;
      memorySemantics |= spv::MemorySemanticsUniformMemoryMask;
    }
    
    if (executionScope != 0) {
      m_module.opControlBarrier(
        m_module.constu32(executionScope),
        m_module.constu32(memoryScope),
        m_module.constu32(memorySemantics));
    } else if (memorySemantics != spv::MemorySemanticsMaskNone) {
      m_module.opMemoryBarrier(
        m_module.constu32(memoryScope),
        m_module.constu32(memorySemantics));
    } else {
      Logger::warn("DxbcCompiler: sync instruction has no effect");
    }
  }
  
  
  void DxbcCompiler::emitBitExtract(const DxbcShaderInstruction& ins) {
    // ibfe and ubfe take the following arguments:
    //    (dst0) The destination register
    //    (src0) Number of bits to extact
    //    (src1) Offset of the bits to extract
    //    (src2) Register to extract bits from
    const bool isSigned = ins.op == DxbcOpcode::IBfe;
    
    const DxbcRegisterValue bitCnt = emitRegisterLoad(ins.src[0], ins.dst[0].mask);
    const DxbcRegisterValue bitOfs = emitRegisterLoad(ins.src[1], ins.dst[0].mask);
    
    const DxbcRegisterValue src = emitRegisterLoad(ins.src[2], ins.dst[0].mask);
    
    const uint32_t componentCount  = src.type.ccount;
    std::array<uint32_t, 4> componentIds = {{ 0, 0, 0, 0 }};
    
    for (uint32_t i = 0; i < componentCount; i++) {
      const DxbcRegisterValue currBitCnt = emitRegisterExtract(bitCnt, DxbcRegMask::select(i));
      const DxbcRegisterValue currBitOfs = emitRegisterExtract(bitOfs, DxbcRegMask::select(i));
      const DxbcRegisterValue currSrc    = emitRegisterExtract(src,    DxbcRegMask::select(i));
      
      const uint32_t typeId = getVectorTypeId(currSrc.type);
      
      componentIds[i] = isSigned
        ? m_module.opBitFieldSExtract(typeId, currSrc.id, currBitOfs.id, currBitCnt.id)
        : m_module.opBitFieldUExtract(typeId, currSrc.id, currBitOfs.id, currBitCnt.id);
    }
    
    DxbcRegisterValue result;
    result.type = src.type;
    result.id   = componentCount > 1
      ? m_module.opCompositeConstruct(
          getVectorTypeId(result.type),
          componentCount, componentIds.data())
      : componentIds[0];
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitBitInsert(const DxbcShaderInstruction& ins) {
    // ibfe and ubfe take the following arguments:
    //    (dst0) The destination register
    //    (src0) Number of bits to extact
    //    (src1) Offset of the bits to extract
    //    (src2) Register to take bits from
    //    (src3) Register to replace bits in
    const DxbcRegisterValue bitCnt = emitRegisterLoad(ins.src[0], ins.dst[0].mask);
    const DxbcRegisterValue bitOfs = emitRegisterLoad(ins.src[1], ins.dst[0].mask);
    
    const DxbcRegisterValue insert = emitRegisterLoad(ins.src[2], ins.dst[0].mask);
    const DxbcRegisterValue base   = emitRegisterLoad(ins.src[3], ins.dst[0].mask);
    
    const uint32_t componentCount  = base.type.ccount;
    std::array<uint32_t, 4> componentIds = {{ 0, 0, 0, 0 }};
    
    for (uint32_t i = 0; i < componentCount; i++) {
      const DxbcRegisterValue currBitCnt = emitRegisterExtract(bitCnt, DxbcRegMask::select(i));
      const DxbcRegisterValue currBitOfs = emitRegisterExtract(bitOfs, DxbcRegMask::select(i));
      const DxbcRegisterValue currInsert = emitRegisterExtract(insert, DxbcRegMask::select(i));
      const DxbcRegisterValue currBase   = emitRegisterExtract(base,   DxbcRegMask::select(i));
      
      componentIds[i] = m_module.opBitFieldInsert(
        getVectorTypeId(currBase.type),
        currBase.id,   currInsert.id,
        currBitOfs.id, currBitCnt.id);
    }
    
    DxbcRegisterValue result;
    result.type = base.type;
    result.id   = componentCount > 1
      ? m_module.opCompositeConstruct(
          getVectorTypeId(result.type),
          componentCount, componentIds.data())
      : componentIds[0];
    emitRegisterStore(ins.dst[0], result);
  }
  
    
  void DxbcCompiler::emitBufferQuery(const DxbcShaderInstruction& ins) {
    // bufinfo takes two arguments
    //    (dst0) The destination register
    //    (src0) The buffer register to query
    // TODO Check if resource is bound
    const DxbcBufferInfo bufferInfo = getBufferInfo(ins.src[0]);
    
    // We'll store this as a scalar unsigned integer
    DxbcRegisterValue result = emitQueryTexelBufferSize(ins.src[0]);
    const uint32_t typeId = getVectorTypeId(result.type);
    
    // Adjust returned size if this is a raw or structured
    // buffer, as emitQueryTexelBufferSize only returns the
    // number of typed elements in the buffer.
    if (bufferInfo.type == DxbcResourceType::Raw) {
      result.id = m_module.opIMul(typeId,
        result.id, m_module.constu32(4));
    } else if (bufferInfo.type == DxbcResourceType::Structured) {
      result.id = m_module.opUDiv(typeId, result.id,
        m_module.constu32(bufferInfo.stride / 4));
    }
    
    // Store the result. The scalar will be extended to a
    // vector if the write mask consists of more than one
    // component, which is the desired behaviour.
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitBufferLoad(const DxbcShaderInstruction& ins) {
    // ld_raw takes three arguments:
    //    (dst0) Destination register
    //    (src0) Byte offset
    //    (src1) Source register
    // ld_structured takes four arguments:
    //    (dst0) Destination register
    //    (src0) Structure index
    //    (src1) Byte offset
    //    (src2) Source register
    // TODO Check if resource is bound
    const bool isStructured = ins.op == DxbcOpcode::LdStructured;
    
    // Source register. The exact way we access
    // the data depends on the register type.
    const DxbcRegister& dstReg = ins.dst[0];
    const DxbcRegister& srcReg = isStructured ? ins.src[2] : ins.src[1];
    
    // Retrieve common info about the buffer
    const DxbcBufferInfo bufferInfo = getBufferInfo(srcReg);
    
    // Compute element index
    const DxbcRegisterValue elementIndex = isStructured
      ? emitCalcBufferIndexStructured(
          emitRegisterLoad(ins.src[0], DxbcRegMask(true, false, false, false)),
          emitRegisterLoad(ins.src[1], DxbcRegMask(true, false, false, false)),
          bufferInfo.stride)
      : emitCalcBufferIndexRaw(
          emitRegisterLoad(ins.src[0], DxbcRegMask(true, false, false, false)));
    
    emitRegisterStore(dstReg,
      emitRawBufferLoad(srcReg, elementIndex, dstReg.mask));
  }
  
  
  void DxbcCompiler::emitBufferStore(const DxbcShaderInstruction& ins) {
    // store_raw takes three arguments:
    //    (dst0) Destination register
    //    (src0) Byte offset
    //    (src1) Source register
    // store_structured takes four arguments:
    //    (dst0) Destination register
    //    (src0) Structure index
    //    (src1) Byte offset
    //    (src2) Source register
    // TODO Check if resource is bound
    const bool isStructured = ins.op == DxbcOpcode::StoreStructured;
    
    // Source register. The exact way we access
    // the data depends on the register type.
    const DxbcRegister& dstReg = ins.dst[0];
    const DxbcRegister& srcReg = isStructured ? ins.src[2] : ins.src[1];
    
    // Retrieve common info about the buffer
    const DxbcBufferInfo bufferInfo = getBufferInfo(dstReg);
    
    // Compute element index
    const DxbcRegisterValue elementIndex = isStructured
      ? emitCalcBufferIndexStructured(
          emitRegisterLoad(ins.src[0], DxbcRegMask(true, false, false, false)),
          emitRegisterLoad(ins.src[1], DxbcRegMask(true, false, false, false)),
          bufferInfo.stride)
      : emitCalcBufferIndexRaw(
          emitRegisterLoad(ins.src[0], DxbcRegMask(true, false, false, false)));
    
    emitRawBufferStore(dstReg, elementIndex,
      emitRegisterLoad(srcReg, dstReg.mask));
  }
  
  
  void DxbcCompiler::emitConvertFloat16(const DxbcShaderInstruction& ins) {
    // f32tof16 takes two operands:
    //    (dst0) Destination register as a uint32 vector
    //    (src0) Source register as a float32 vector
    // f16tof32 takes two operands:
    //    (dst0) Destination register as a float32 vector
    //    (src0) Source register as a uint32 vector
    const DxbcRegisterValue src = emitRegisterLoad(ins.src[0], ins.dst[0].mask);
    
    // We handle both packing and unpacking here
    const bool isPack = ins.op == DxbcOpcode::F32toF16;
    
    // The conversion instructions do not map very well to the
    // SPIR-V pack instructions, which operate on 2D vectors.
    std::array<uint32_t, 4> scalarIds  = {{ 0, 0, 0, 0 }};
    
    const uint32_t componentCount = src.type.ccount;
    
    // These types are used in both pack and unpack operations
    const uint32_t t_u32   = getVectorTypeId({ DxbcScalarType::Uint32,  1 });
    const uint32_t t_f32   = getVectorTypeId({ DxbcScalarType::Float32, 1 });
    const uint32_t t_f32v2 = getVectorTypeId({ DxbcScalarType::Float32, 2 });
    
    // Constant zero-bit pattern, used for packing
    const uint32_t zerof32 = isPack ? m_module.constf32(0.0f) : 0;
    
    for (uint32_t i = 0; i < componentCount; i++) {
      const DxbcRegisterValue componentValue
        = emitRegisterExtract(src, DxbcRegMask::select(i));
      
      if (isPack) {  // f32tof16
        const std::array<uint32_t, 2> packIds =
          {{ componentValue.id, zerof32 }};
        
        scalarIds[i] = m_module.opPackHalf2x16(t_u32,
          m_module.opCompositeConstruct(t_f32v2, packIds.size(), packIds.data()));
      } else {  // f16tof32
        const uint32_t zeroIndex = 0;
        
        scalarIds[i] = m_module.opCompositeExtract(t_f32,
          m_module.opUnpackHalf2x16(t_f32v2, componentValue.id),
          1, &zeroIndex);
      }
    }
    
    // Store result in the destination register
    DxbcRegisterValue result;
    result.type.ctype  = ins.dst[0].dataType;
    result.type.ccount = componentCount;
    result.id = componentCount > 1
      ? m_module.opCompositeConstruct(
          getVectorTypeId(result.type),
          componentCount, scalarIds.data())
      : scalarIds[0];
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitTextureQuery(const DxbcShaderInstruction& ins) {
    // resinfo has three operands:
    //    (dst0) The destination register
    //    (src0) Resource LOD to query
    //    (src1) Resource to query
    // TODO Check if resource is bound
    const DxbcBufferInfo resourceInfo = getBufferInfo(ins.src[1]);
    const DxbcResinfoType resinfoType = ins.controls.resinfoType;
    
    // Read the exact LOD for the image query
    const DxbcRegisterValue mipLod = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, false, false, false));
    
    const DxbcScalarType returnType = resinfoType == DxbcResinfoType::Uint
      ? DxbcScalarType::Uint32 : DxbcScalarType::Float32;
    
    // Query the size of the selected mip level, as well as the
    // total number of mip levels. We will have to combine the
    // result into a four-component vector later.
    DxbcRegisterValue imageSize   = emitQueryTextureSize(ins.src[1], mipLod);
    DxbcRegisterValue imageLevels = emitQueryTextureLods(ins.src[1]);
    
    // Convert intermediates to the requested type
    if (returnType == DxbcScalarType::Float32) {
      imageSize.type.ctype = DxbcScalarType::Float32;
      imageSize.id = m_module.opConvertUtoF(
        getVectorTypeId(imageSize.type),
        imageSize.id);
      
      imageLevels.type.ctype = DxbcScalarType::Float32;
      imageLevels.id = m_module.opConvertUtoF(
        getVectorTypeId(imageLevels.type),
        imageLevels.id);
    }
    
    // If the selected return type is rcpFloat, we need
    // to compute the reciprocal of the image dimensions,
    // but not the array size, so we need to separate it.
    const uint32_t imageCoordDim = imageSize.type.ccount;
    
    DxbcRegisterValue imageLayers;
    imageLayers.type = imageSize.type;
    imageLayers.id   = 0;
    
    if (resinfoType == DxbcResinfoType::RcpFloat && resourceInfo.image.array) {
      imageLayers = emitRegisterExtract(imageSize, DxbcRegMask::select(imageCoordDim - 1));
      imageSize   = emitRegisterExtract(imageSize, DxbcRegMask::firstN(imageCoordDim - 1));
    }
    
    if (resinfoType == DxbcResinfoType::RcpFloat) {
      const uint32_t typeId = getVectorTypeId(imageSize.type);
      
      const uint32_t one = m_module.constf32(1.0f);
      std::array<uint32_t, 4> constIds = { one, one, one, one };
      
      imageSize.id = m_module.opFDiv(typeId,
        m_module.constComposite(typeId,
          imageSize.type.ccount, constIds.data()),
        imageSize.id);
    }
    
    // Concatenate result vectors and scalars to form a
    // 4D vector. Unused components will be set to zero.
    std::array<uint32_t, 4> vectorIds = { imageSize.id, 0, 0, 0 };
    uint32_t numVectorIds = 1;
    
    if (imageLayers.id != 0)
      vectorIds[numVectorIds++] = imageLayers.id;
    
    if (imageCoordDim < 3) {
      const uint32_t zero = returnType == DxbcScalarType::Uint32
        ? m_module.constu32(0)
        : m_module.constf32(0.0f);
      
      for (uint32_t i = imageCoordDim; i < 3; i++)
        vectorIds[numVectorIds++] = zero;
    }
    
    vectorIds[numVectorIds++] = imageLevels.id;
    
    // Create the actual result vector
    DxbcRegisterValue result;
    result.type.ctype  = returnType;
    result.type.ccount = 4;
    result.id = m_module.opCompositeConstruct(
      getVectorTypeId(result.type),
      numVectorIds, vectorIds.data());
    
    // Swizzle components using the resource swizzle
    // and the destination operand's write mask
    result = emitRegisterSwizzle(result,
      ins.src[1].swizzle, ins.dst[0].mask);
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitTextureQueryLod(const DxbcShaderInstruction& ins) {
    // All sample instructions have at least these operands:
    //    (dst0) The destination register
    //    (src0) Texture coordinates
    //    (src1) The texture itself
    //    (src2) The sampler object
    const DxbcRegister& texCoordReg = ins.src[0];
    const DxbcRegister& textureReg  = ins.src[1];
    const DxbcRegister& samplerReg  = ins.src[2];
    
    // Texture and sampler register IDs
    const uint32_t textureId = textureReg.idx[0].offset;
    const uint32_t samplerId = samplerReg.idx[0].offset;
    
    // Load texture coordinates 
    const uint32_t imageCoordDim = getTexCoordDim(
      m_textures.at(textureId).imageInfo);
    
    const DxbcRegisterValue coord = emitRegisterLoad(
      texCoordReg, DxbcRegMask::firstN(imageCoordDim));
    
    // Query the LOD. The result is a two-dimensional float32
    // vector containing the mip level and virtual LOD numbers.
    const uint32_t sampledImageId = emitLoadSampledImage(
      m_textures.at(textureId), m_samplers.at(samplerId), false);
    
    const uint32_t queriedLodId = m_module.opImageQueryLod(
      getVectorTypeId({ DxbcScalarType::Float32, 2 }),
      sampledImageId, coord.id);
    
    // Build the result array vector by filling up
    // the remaining two components with zeroes.
    const uint32_t zero = m_module.constf32(0.0f);
    const std::array<uint32_t, 3> resultIds
      = {{ queriedLodId, zero, zero }};
    
    DxbcRegisterValue result;
    result.type = DxbcVectorType { DxbcScalarType::Float32, 4 };
    result.id   = m_module.opCompositeConstruct(
      getVectorTypeId(result.type),
      resultIds.size(), resultIds.data());
    
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitTextureQueryMs(const DxbcShaderInstruction& ins) {
    // sampleinfo has two operands:
    //    (dst0) The destination register
    //    (src0) Resource to query
    // TODO Check if resource is bound
    DxbcRegisterValue sampleCount = emitQueryTextureSamples(ins.src[0]);
    
    if (ins.controls.resinfoType != DxbcResinfoType::Uint) {
      sampleCount.type.ctype  = DxbcScalarType::Float32;
      sampleCount.type.ccount = 1;
      sampleCount.id = m_module.opConvertUtoF(
        getVectorTypeId(sampleCount.type),
        sampleCount.id);
    }
    
    emitRegisterStore(ins.dst[0], sampleCount);
  }
  
  
  void DxbcCompiler::emitTextureFetch(const DxbcShaderInstruction& ins) {
    // ld has three operands:
    //    (dst0) The destination register
    //    (src0) Source address
    //    (src1) Source texture
    // ld2dms has four operands:
    //    (dst0) The destination register
    //    (src0) Source address
    //    (src1) Source texture
    //    (src2) Sample number
    const uint32_t textureId = ins.src[1].idx[0].offset;
    
    // Image type, which stores the image dimensions etc.
    const DxbcImageInfo imageType      = m_textures.at(textureId).imageInfo;
    const DxbcRegMask   coordArrayMask = getTexCoordMask(imageType);
    
    const uint32_t imageLayerDim = getTexLayerDim(imageType);
    
    // Load the texture coordinates. The last component
    // contains the LOD if the resource is an image.
    const DxbcRegisterValue address = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, true, true, true));
    
    // Additional image operands. This will store
    // the LOD and the address offset if present.
    SpirvImageOperands imageOperands;
    
    if (ins.sampleControls.u != 0 || ins.sampleControls.v != 0 || ins.sampleControls.w != 0) {
      const std::array<uint32_t, 3> offsetIds = {
        imageLayerDim >= 1 ? m_module.consti32(ins.sampleControls.u) : 0,
        imageLayerDim >= 2 ? m_module.consti32(ins.sampleControls.v) : 0,
        imageLayerDim >= 3 ? m_module.consti32(ins.sampleControls.w) : 0,
      };
      
      imageOperands.flags |= spv::ImageOperandsConstOffsetMask;
      imageOperands.sConstOffset = m_module.constComposite(
        getVectorTypeId({ DxbcScalarType::Sint32, imageLayerDim }),
        imageLayerDim, offsetIds.data());
    }
    
    // The LOD is not present when reading from
    // a buffer or from a multisample texture.
    if (imageType.dim != spv::DimBuffer && imageType.ms == 0) {
      DxbcRegisterValue imageLod = emitRegisterExtract(
        address, DxbcRegMask(false, false, false, true));
      
      imageOperands.flags |= spv::ImageOperandsLodMask;
      imageOperands.sLod = imageLod.id;
    }
    
    // The ld2ms instruction has a sample index, but we
    // are only allowed to set it for multisample views
    if (ins.op == DxbcOpcode::LdMs && imageType.ms == 1) {
      DxbcRegisterValue sampleId = emitRegisterLoad(
        ins.src[2], DxbcRegMask(true, false, false, false));
      
      imageOperands.flags |= spv::ImageOperandsSampleMask;
      imageOperands.sSampleId = sampleId.id;
    }
    
    // Extract coordinates from address
    const DxbcRegisterValue coord =
      emitRegisterExtract(address, coordArrayMask);
    
    // Fetch texels only if the resource is actually bound
    const uint32_t labelMerge     = m_module.allocateId();
    const uint32_t labelBound     = m_module.allocateId();
    const uint32_t labelUnbound   = m_module.allocateId();
    
    m_module.opSelectionMerge(labelMerge, spv::SelectionControlMaskNone);
    m_module.opBranchConditional(m_textures.at(textureId).specId, labelBound, labelUnbound);
    m_module.opLabel(labelBound);
    
    // Reading a typed image or buffer view
    // always returns a four-component vector.
    const uint32_t imageId = m_module.opLoad(
      m_textures.at(textureId).imageTypeId,
      m_textures.at(textureId).varId);
    
    DxbcRegisterValue result;
    result.type.ctype  = m_textures.at(textureId).sampledType;
    result.type.ccount = 4;
    result.id = m_module.opImageFetch(
      getVectorTypeId(result.type), imageId,
      coord.id, imageOperands);
    
    // Swizzle components using the texture swizzle
    // and the destination operand's write mask
    result = emitRegisterSwizzle(result,
      ins.src[1].swizzle, ins.dst[0].mask);

    // If the texture is not bound, return zeroes
    m_module.opBranch(labelMerge);
    m_module.opLabel(labelUnbound);
    
    DxbcRegisterValue zeroes = [&] {
      switch (result.type.ctype) {
        case DxbcScalarType::Float32: return emitBuildConstVecf32(0.0f, 0.0f, 0.0f, 0.0f, ins.dst[0].mask);
        case DxbcScalarType::Uint32:  return emitBuildConstVecu32(0u, 0u, 0u, 0u,         ins.dst[0].mask);
        case DxbcScalarType::Sint32:  return emitBuildConstVeci32(0, 0, 0, 0,             ins.dst[0].mask);
        default: throw DxvkError("DxbcCompiler: Invalid scalar type");
      }
    }();
    
    m_module.opBranch(labelMerge);
    m_module.opLabel(labelMerge);
    
    // Merge the result with a phi function
    const std::array<SpirvPhiLabel, 2> phiLabels = {{
      { result.id, labelBound   },
      { zeroes.id, labelUnbound },
    }};
    
    DxbcRegisterValue mergedResult;
    mergedResult.type = result.type;
    mergedResult.id = m_module.opPhi(
      getVectorTypeId(mergedResult.type),
      phiLabels.size(), phiLabels.data());
    
    emitRegisterStore(ins.dst[0], mergedResult);
  }
  
  
  void DxbcCompiler::emitTextureGather(const DxbcShaderInstruction& ins) {
    // Gather4 takes the following operands:
    //    (dst0) The destination register
    //    (src0) Texture coordinates
    //    (src1) The texture itself
    //    (src2) The sampler, with a component selector
    // Gather4C takes the following additional operand:
    //    (src3) The depth reference value
    // The Gather4Po variants take an additional operand
    // which defines an extended constant offset.
    // TODO reduce code duplication by moving some common code
    // in both sample() and gather() into separate methods
    const bool isExtendedGather = ins.op == DxbcOpcode::Gather4Po
                               || ins.op == DxbcOpcode::Gather4PoC;
    
    const DxbcRegister& texCoordReg = ins.src[0];
    const DxbcRegister& textureReg  = ins.src[1 + isExtendedGather];
    const DxbcRegister& samplerReg  = ins.src[2 + isExtendedGather];
    
    // Texture and sampler register IDs
    const uint32_t textureId = textureReg.idx[0].offset;
    const uint32_t samplerId = samplerReg.idx[0].offset;
    
    // Image type, which stores the image dimensions etc.
    const DxbcImageInfo imageType = m_textures.at(textureId).imageInfo;
    
    const uint32_t imageLayerDim = getTexLayerDim(imageType);
    const uint32_t imageCoordDim = getTexCoordDim(imageType);
    
    const DxbcRegMask coordArrayMask = DxbcRegMask::firstN(imageCoordDim);
    
    // Load the texture coordinates. SPIR-V allows these
    // to be float4 even if not all components are used.
    DxbcRegisterValue coord = emitRegisterLoad(texCoordReg, coordArrayMask);
    
    // Load reference value for depth-compare operations
    const bool isDepthCompare = ins.op == DxbcOpcode::Gather4C
                             || ins.op == DxbcOpcode::Gather4PoC;
    
    const DxbcRegisterValue referenceValue = isDepthCompare
      ? emitRegisterLoad(ins.src[3 + isExtendedGather],
          DxbcRegMask(true, false, false, false))
      : DxbcRegisterValue();
    
    if (isDepthCompare && m_options.packDrefValueIntoCoordinates) {
      const std::array<uint32_t, 2> packedCoordIds
        = {{ coord.id, referenceValue.id }};
      
      coord.type.ccount += 1;
      coord.id = m_module.opCompositeConstruct(
        getVectorTypeId(coord.type),
        packedCoordIds.size(),
        packedCoordIds.data());
    }
    
    // Determine the sampled image type based on the opcode.
    const uint32_t sampledImageType = isDepthCompare
      ? m_module.defSampledImageType(m_textures.at(textureId).depthTypeId)
      : m_module.defSampledImageType(m_textures.at(textureId).colorTypeId);
    
    // Accumulate additional image operands.
    SpirvImageOperands imageOperands;
    
    if (isExtendedGather) {
      m_module.enableCapability(spv::CapabilityImageGatherExtended);
      
      DxbcRegisterValue gatherOffset = emitRegisterLoad(
        ins.src[1], DxbcRegMask::firstN(imageLayerDim));
      
      imageOperands.flags |= spv::ImageOperandsOffsetMask;
      imageOperands.gOffset = gatherOffset.id;
    } else if (ins.sampleControls.u != 0 || ins.sampleControls.v != 0 || ins.sampleControls.w != 0) {
      const std::array<uint32_t, 3> offsetIds = {
        imageLayerDim >= 1 ? m_module.consti32(ins.sampleControls.u) : 0,
        imageLayerDim >= 2 ? m_module.consti32(ins.sampleControls.v) : 0,
        imageLayerDim >= 3 ? m_module.consti32(ins.sampleControls.w) : 0,
      };
      
      imageOperands.flags |= spv::ImageOperandsConstOffsetMask;
      imageOperands.sConstOffset = m_module.constComposite(
        getVectorTypeId({ DxbcScalarType::Sint32, imageLayerDim }),
        imageLayerDim, offsetIds.data());
    }
    
    // Combine the texture and the sampler into a sampled image
    const uint32_t sampledImageId = m_module.opSampledImage(
      sampledImageType,
      m_module.opLoad(
        m_textures.at(textureId).imageTypeId,
        m_textures.at(textureId).varId),
      m_module.opLoad(
        m_samplers.at(samplerId).typeId,
        m_samplers.at(samplerId).varId));
    
    // Gathering texels always returns a four-component
    // vector, even for the depth-compare variants.
    DxbcRegisterValue result;
    result.type.ctype  = m_textures.at(textureId).sampledType;
    result.type.ccount = 4;
    
    switch (ins.op) {
      // Simple image gather operation
      case DxbcOpcode::Gather4:
      case DxbcOpcode::Gather4Po: {
        result.id = m_module.opImageGather(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          m_module.constu32(samplerReg.swizzle[0]),
          imageOperands);
      } break;
      
      // Depth-compare operation
      case DxbcOpcode::Gather4C:
      case DxbcOpcode::Gather4PoC: {
        result.id = m_module.opImageDrefGather(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          referenceValue.id, imageOperands);
      } break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    // Swizzle components using the texture swizzle
    // and the destination operand's write mask
    result = emitRegisterSwizzle(result,
      textureReg.swizzle, ins.dst[0].mask);
    
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitTextureSample(const DxbcShaderInstruction& ins) {
    // All sample instructions have at least these operands:
    //    (dst0) The destination register
    //    (src0) Texture coordinates
    //    (src1) The texture itself
    //    (src2) The sampler object
    const DxbcRegister& texCoordReg = ins.src[0];
    const DxbcRegister& textureReg  = ins.src[1];
    const DxbcRegister& samplerReg  = ins.src[2];
    
    // Texture and sampler register IDs
    const uint32_t textureId = textureReg.idx[0].offset;
    const uint32_t samplerId = samplerReg.idx[0].offset;
    
    // Image type, which stores the image dimensions etc.
    const DxbcImageInfo imageType = m_textures.at(textureId).imageInfo;
    
    const uint32_t imageLayerDim = getTexLayerDim(imageType);
    const uint32_t imageCoordDim = getTexCoordDim(imageType);
    
    const DxbcRegMask coordArrayMask = DxbcRegMask::firstN(imageCoordDim);
    const DxbcRegMask coordLayerMask = DxbcRegMask::firstN(imageLayerDim);
    
    // Load the texture coordinates. SPIR-V allows these
    // to be float4 even if not all components are used.
    DxbcRegisterValue coord = emitRegisterLoad(texCoordReg, coordArrayMask);
    
    // Load reference value for depth-compare operations
    const bool isDepthCompare = ins.op == DxbcOpcode::SampleC
                             || ins.op == DxbcOpcode::SampleClz;
    
    const DxbcRegisterValue referenceValue = isDepthCompare
      ? emitRegisterLoad(ins.src[3], DxbcRegMask(true, false, false, false))
      : DxbcRegisterValue();
    
    if (isDepthCompare && m_options.packDrefValueIntoCoordinates) {
      const std::array<uint32_t, 2> packedCoordIds
        = {{ coord.id, referenceValue.id }};
      
      coord.type.ccount += 1;
      coord.id = m_module.opCompositeConstruct(
        getVectorTypeId(coord.type),
        packedCoordIds.size(),
        packedCoordIds.data());
    }
    
    // Load explicit gradients for sample operations that require them
    const bool hasExplicitGradients = ins.op == DxbcOpcode::SampleD;
    
    const DxbcRegisterValue explicitGradientX = hasExplicitGradients
      ? emitRegisterLoad(ins.src[3], coordLayerMask)
      : DxbcRegisterValue();
    
    const DxbcRegisterValue explicitGradientY = hasExplicitGradients
      ? emitRegisterLoad(ins.src[4], coordLayerMask)
      : DxbcRegisterValue();
    
    // LOD for certain sample operations
    const bool hasLod = ins.op == DxbcOpcode::SampleL
                     || ins.op == DxbcOpcode::SampleB;
    
    const DxbcRegisterValue lod = hasLod
      ? emitRegisterLoad(ins.src[3], DxbcRegMask(true, false, false, false))
      : DxbcRegisterValue();
    
    // Accumulate additional image operands. These are
    // not part of the actual operand token in SPIR-V.
    SpirvImageOperands imageOperands;
    
    if (ins.sampleControls.u != 0 || ins.sampleControls.v != 0 || ins.sampleControls.w != 0) {
      const std::array<uint32_t, 3> offsetIds = {
        imageLayerDim >= 1 ? m_module.consti32(ins.sampleControls.u) : 0,
        imageLayerDim >= 2 ? m_module.consti32(ins.sampleControls.v) : 0,
        imageLayerDim >= 3 ? m_module.consti32(ins.sampleControls.w) : 0,
      };
      
      imageOperands.flags |= spv::ImageOperandsConstOffsetMask;
      imageOperands.sConstOffset = m_module.constComposite(
        getVectorTypeId({ DxbcScalarType::Sint32, imageLayerDim }),
        imageLayerDim, offsetIds.data());
    }
    
    // Combine the texture and the sampler into a sampled image
    const uint32_t sampledImageId = emitLoadSampledImage(
      m_textures.at(textureId), m_samplers.at(samplerId),
      isDepthCompare);
    
    // Sampling an image always returns a four-component
    // vector, whereas depth-compare ops return a scalar.
    DxbcRegisterValue result;
    result.type.ctype  = m_textures.at(textureId).sampledType;
    result.type.ccount = isDepthCompare ? 1 : 4;
    
    switch (ins.op) {
      // Simple image sample operation
      case DxbcOpcode::Sample: {
        result.id = m_module.opImageSampleImplicitLod(
          getVectorTypeId(result.type),
          sampledImageId, coord.id,
          imageOperands);
      } break;
      
      // Depth-compare operation
      case DxbcOpcode::SampleC: {
        result.id = m_module.opImageSampleDrefImplicitLod(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          referenceValue.id, imageOperands);
      } break;
      
      // Depth-compare operation on mip level zero
      case DxbcOpcode::SampleClz: {
        imageOperands.flags |= spv::ImageOperandsLodMask;
        imageOperands.sLod = m_module.constf32(0.0f);
        
        result.id = m_module.opImageSampleDrefExplicitLod(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          referenceValue.id, imageOperands);
      } break;
      
      // Sample operation with explicit gradients
      case DxbcOpcode::SampleD: {
        imageOperands.flags |= spv::ImageOperandsGradMask;
        imageOperands.sGradX = explicitGradientX.id;
        imageOperands.sGradY = explicitGradientY.id;
        
        result.id = m_module.opImageSampleExplicitLod(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          imageOperands);
      } break;
      
      // Sample operation with explicit LOD
      case DxbcOpcode::SampleL: {
        imageOperands.flags |= spv::ImageOperandsLodMask;
        imageOperands.sLod = lod.id;
        
        result.id = m_module.opImageSampleExplicitLod(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          imageOperands);
      } break;
      
      // Sample operation with LOD bias
      case DxbcOpcode::SampleB: {
        imageOperands.flags |= spv::ImageOperandsBiasMask;
        imageOperands.sLodBias = lod.id;
        
        result.id = m_module.opImageSampleImplicitLod(
          getVectorTypeId(result.type), sampledImageId, coord.id,
          imageOperands);
      } break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
        return;
    }
    
    // Swizzle components using the texture swizzle
    // and the destination operand's write mask
    if (result.type.ccount != 1) {
      result = emitRegisterSwizzle(result,
        textureReg.swizzle, ins.dst[0].mask);
    }
    
    emitRegisterStore(ins.dst[0], result);
  }
  
  
  void DxbcCompiler::emitTypedUavLoad(const DxbcShaderInstruction& ins) {
    // load_uav_typed has three operands:
    //    (dst0) The destination register
    //    (src0) The texture or buffer coordinates
    //    (src1) The UAV to load from
    const uint32_t registerId = ins.src[1].idx[0].offset;
    const DxbcUav uavInfo = m_uavs.at(registerId);
    
    // Load texture coordinates
    const DxbcRegisterValue texCoord = emitRegisterLoad(
      ins.src[0], getTexCoordMask(uavInfo.imageInfo));
    
    // Load source value from the UAV
    DxbcRegisterValue uavValue;
    uavValue.type.ctype  = uavInfo.sampledType;
    uavValue.type.ccount = 4;
    uavValue.id = m_module.opImageRead(
      getVectorTypeId(uavValue.type),
      m_module.opLoad(uavInfo.imageTypeId, uavInfo.varId),
      texCoord.id, SpirvImageOperands());
    
    // Apply component swizzle and mask
    uavValue = emitRegisterSwizzle(uavValue,
      ins.src[1].swizzle, ins.dst[0].mask);
    
    emitRegisterStore(ins.dst[0], uavValue);
  }
  
  
  void DxbcCompiler::emitTypedUavStore(const DxbcShaderInstruction& ins) {
    // store_uav_typed has three operands:
    //    (dst0) The destination UAV
    //    (src0) The texture or buffer coordinates
    //    (src1) The value to store
    const uint32_t registerId = ins.dst[0].idx[0].offset;
    const DxbcUav uavInfo = m_uavs.at(registerId);
    
    // Load texture coordinates
    const DxbcRegisterValue texCoord = emitRegisterLoad(
      ins.src[0], getTexCoordMask(uavInfo.imageInfo));
    
    // Load the value that will be written to the image. We'll
    // have to cast it to the component type of the image.
    const DxbcRegisterValue texValue = emitRegisterBitcast(
      emitRegisterLoad(ins.src[1], DxbcRegMask(true, true, true, true)),
      uavInfo.sampledType);
    
    // Write the given value to the image
    m_module.opImageWrite(
      m_module.opLoad(uavInfo.imageTypeId, uavInfo.varId),
      texCoord.id, texValue.id, SpirvImageOperands());
  }
  
  
  void DxbcCompiler::emitControlFlowIf(const DxbcShaderInstruction& ins) {
    // Load the first component of the condition
    // operand and perform a zero test on it.
    const DxbcRegisterValue condition = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, false, false, false));
    
    const DxbcRegisterValue zeroTest = emitRegisterZeroTest(
      condition, ins.controls.zeroTest);
    
    // Declare the 'if' block. We do not know if there
    // will be an 'else' block or not, so we'll assume
    // that there is one and leave it empty otherwise.
    DxbcCfgBlock block;
    block.type = DxbcCfgBlockType::If;
    block.b_if.labelIf   = m_module.allocateId();
    block.b_if.labelElse = m_module.allocateId();
    block.b_if.labelEnd  = m_module.allocateId();
    block.b_if.hadElse   = false;
    m_controlFlowBlocks.push_back(block);
    
    m_module.opSelectionMerge(
      block.b_if.labelEnd,
      spv::SelectionControlMaskNone);
    
    m_module.opBranchConditional(
      zeroTest.id,
      block.b_if.labelIf,
      block.b_if.labelElse);
    
    m_module.opLabel(block.b_if.labelIf);
  }
  
  
  void DxbcCompiler::emitControlFlowElse(const DxbcShaderInstruction& ins) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxbcCfgBlockType::If
     || m_controlFlowBlocks.back().b_if.hadElse)
      throw DxvkError("DxbcCompiler: 'Else' without 'If' found");
    
    // Set the 'Else' flag so that we do
    // not insert a dummy block on 'EndIf'
    DxbcCfgBlock& block = m_controlFlowBlocks.back();
    block.b_if.hadElse = true;
    
    // Close the 'If' block by branching to
    // the merge block we declared earlier
    m_module.opBranch(block.b_if.labelEnd);
    m_module.opLabel (block.b_if.labelElse);
  }
  
  
  void DxbcCompiler::emitControlFlowEndIf(const DxbcShaderInstruction& ins) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxbcCfgBlockType::If)
      throw DxvkError("DxbcCompiler: 'EndIf' without 'If' found");
    
    // Remove the block from the stack, it's closed
    const DxbcCfgBlock block = m_controlFlowBlocks.back();
    m_controlFlowBlocks.pop_back();
    
    // End the active 'if' or 'else' block
    m_module.opBranch(block.b_if.labelEnd);
    
    // If there was no 'else' block in this construct, we still
    // have to declare it because we used it as a branch target.
    if (!block.b_if.hadElse) {
      m_module.opLabel (block.b_if.labelElse);
      m_module.opBranch(block.b_if.labelEnd);
    }
    
    // Declare the merge block
    m_module.opLabel(block.b_if.labelEnd);
  }
  
  
  void DxbcCompiler::emitControlFlowSwitch(const DxbcShaderInstruction& ins) {
    // Load the selector as a scalar unsigned integer
    const DxbcRegisterValue selector = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, false, false, false));
    
    // Declare switch block. We cannot insert the switch
    // instruction itself yet because the number of case
    // statements and blocks is unknown at this point.
    DxbcCfgBlock block;
    block.type = DxbcCfgBlockType::Switch;
    block.b_switch.insertPtr    = m_module.getInsertionPtr();
    block.b_switch.selectorId   = selector.id;
    block.b_switch.labelBreak   = m_module.allocateId();
    block.b_switch.labelCase    = m_module.allocateId();
    block.b_switch.labelDefault = 0;
    block.b_switch.labelCases   = nullptr;
    m_controlFlowBlocks.push_back(block);
    
    // Define the first 'case' label
    m_module.opLabel(block.b_switch.labelCase);
  }
  
  
  void DxbcCompiler::emitControlFlowCase(const DxbcShaderInstruction& ins) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxbcCfgBlockType::Switch)
      throw DxvkError("DxbcCompiler: 'Case' without 'Switch' found");
    
    // The source operand must be a 32-bit immediate.
    if (ins.src[0].type != DxbcOperandType::Imm32)
      throw DxvkError("DxbcCompiler: Invalid operand type for 'Case'");
    
    // Use the last label allocated for 'case'. The block starting
    // with that label is guaranteed to be empty unless a previous
    // 'case' block was not properly closed in the DXBC shader.
    DxbcCfgBlockSwitch* block = &m_controlFlowBlocks.back().b_switch;
    
    DxbcSwitchLabel label;
    label.desc.literal = ins.src[0].imm.u32_1;
    label.desc.labelId = block->labelCase;
    label.next = block->labelCases;
    block->labelCases = new DxbcSwitchLabel(label);
  }
  
  
  void DxbcCompiler::emitControlFlowDefault(const DxbcShaderInstruction& ins) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxbcCfgBlockType::Switch)
      throw DxvkError("DxbcCompiler: 'Default' without 'Switch' found");
    
    // Set the last label allocated for 'case' as the default label.
    m_controlFlowBlocks.back().b_switch.labelDefault
      = m_controlFlowBlocks.back().b_switch.labelCase;
  }
  
  
  void DxbcCompiler::emitControlFlowEndSwitch(const DxbcShaderInstruction& ins) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxbcCfgBlockType::Switch)
      throw DxvkError("DxbcCompiler: 'EndSwitch' without 'Switch' found");
    
    // Remove the block from the stack, it's closed
    DxbcCfgBlock block = m_controlFlowBlocks.back();
    m_controlFlowBlocks.pop_back();
    
    // If no 'default' label was specified, use the last allocated
    // 'case' label. This is guaranteed to be an empty block unless
    // a previous 'case' block was not closed properly.
    if (block.b_switch.labelDefault == 0)
      block.b_switch.labelDefault = block.b_switch.labelCase;
    
    // Close the current 'case' block
    m_module.opBranch(block.b_switch.labelBreak);
    m_module.opLabel (block.b_switch.labelBreak);
    
    // Insert the 'switch' statement. For that, we need to
    // gather all the literal-label pairs for the construct.
    m_module.beginInsertion(block.b_switch.insertPtr);
    m_module.opSelectionMerge(
      block.b_switch.labelBreak,
      spv::SelectionControlMaskNone);
    
    // We'll restore the original order of the case labels here
    std::vector<SpirvSwitchCaseLabel> jumpTargets;
    for (auto i = block.b_switch.labelCases; i != nullptr; i = i->next)
      jumpTargets.insert(jumpTargets.begin(), i->desc);
    
    m_module.opSwitch(
      block.b_switch.selectorId,
      block.b_switch.labelDefault,
      jumpTargets.size(),
      jumpTargets.data());
    m_module.endInsertion();
    
    // Destroy the list of case labels
    // FIXME we're leaking memory if compilation fails.
    DxbcSwitchLabel* caseLabel = block.b_switch.labelCases;
    
    while (caseLabel != nullptr)
      delete std::exchange(caseLabel, caseLabel->next);
  }
  
    
  void DxbcCompiler::emitControlFlowLoop(const DxbcShaderInstruction& ins) {
    // Declare the 'loop' block
    DxbcCfgBlock block;
    block.type = DxbcCfgBlockType::Loop;
    block.b_loop.labelHeader   = m_module.allocateId();
    block.b_loop.labelBegin    = m_module.allocateId();
    block.b_loop.labelContinue = m_module.allocateId();
    block.b_loop.labelBreak    = m_module.allocateId();
    m_controlFlowBlocks.push_back(block);
    
    m_module.opBranch(block.b_loop.labelHeader);
    m_module.opLabel (block.b_loop.labelHeader);
    
    m_module.opLoopMerge(
      block.b_loop.labelBreak,
      block.b_loop.labelContinue,
      spv::LoopControlMaskNone);
    
    m_module.opBranch(block.b_loop.labelBegin);
    m_module.opLabel (block.b_loop.labelBegin);
  }
  
  
  void DxbcCompiler::emitControlFlowEndLoop(const DxbcShaderInstruction& ins) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxbcCfgBlockType::Loop)
      throw DxvkError("DxbcCompiler: 'EndLoop' without 'Loop' found");
    
    // Remove the block from the stack, it's closed
    const DxbcCfgBlock block = m_controlFlowBlocks.back();
    m_controlFlowBlocks.pop_back();
    
    // Declare the continue block
    m_module.opBranch(block.b_loop.labelContinue);
    m_module.opLabel (block.b_loop.labelContinue);
    
    // Declare the merge block
    m_module.opBranch(block.b_loop.labelHeader);
    m_module.opLabel (block.b_loop.labelBreak);
  }
  
  
  void DxbcCompiler::emitControlFlowBreak(const DxbcShaderInstruction& ins) {
    const bool isBreak = ins.op == DxbcOpcode::Break;
    
    DxbcCfgBlock* cfgBlock = isBreak
      ? cfgFindBlock({ DxbcCfgBlockType::Loop, DxbcCfgBlockType::Switch })
      : cfgFindBlock({ DxbcCfgBlockType::Loop });
    
    if (cfgBlock == nullptr)
      throw DxvkError("DxbcCompiler: 'Break' or 'Continue' outside 'Loop' or 'Switch' found");
    
    if (cfgBlock->type == DxbcCfgBlockType::Loop) {
      m_module.opBranch(isBreak
        ? cfgBlock->b_loop.labelBreak
        : cfgBlock->b_loop.labelContinue);
    } else /* if (cfgBlock->type == DxbcCfgBlockType::Switch) */ {
      m_module.opBranch(cfgBlock->b_switch.labelBreak);
    }
    
    // Subsequent instructions assume that there is an open block
    const uint32_t labelId = m_module.allocateId();
    m_module.opLabel(labelId);
    
    // If this is on the same level as a switch-case construct,
    // rather than being nested inside an 'if' statement, close
    // the current 'case' block.
    if (m_controlFlowBlocks.back().type == DxbcCfgBlockType::Switch)
      cfgBlock->b_switch.labelCase = labelId;
  }
  
  
  void DxbcCompiler::emitControlFlowBreakc(const DxbcShaderInstruction& ins) {
    const bool isBreak = ins.op == DxbcOpcode::Breakc;
    
    DxbcCfgBlock* cfgBlock = isBreak
      ? cfgFindBlock({ DxbcCfgBlockType::Loop, DxbcCfgBlockType::Switch })
      : cfgFindBlock({ DxbcCfgBlockType::Loop });
    
    if (cfgBlock == nullptr)
      throw DxvkError("DxbcCompiler: 'Breakc' or 'Continuec' outside 'Loop' or 'Switch' found");
    
    // Perform zero test on the first component of the condition
    const DxbcRegisterValue condition = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, false, false, false));
    
    const DxbcRegisterValue zeroTest = emitRegisterZeroTest(
      condition, ins.controls.zeroTest);
    
    // We basically have to wrap this into an 'if' block
    const uint32_t breakBlock = m_module.allocateId();
    const uint32_t mergeBlock = m_module.allocateId();
    
    m_module.opSelectionMerge(mergeBlock,
      spv::SelectionControlMaskNone);
    
    m_module.opBranchConditional(
      zeroTest.id, breakBlock, mergeBlock);
    
    m_module.opLabel(breakBlock);
    
    if (cfgBlock->type == DxbcCfgBlockType::Loop) {
      m_module.opBranch(isBreak
        ? cfgBlock->b_loop.labelBreak
        : cfgBlock->b_loop.labelContinue);
    } else /* if (cfgBlock->type == DxbcCfgBlockType::Switch) */ {
      m_module.opBranch(cfgBlock->b_switch.labelBreak);
    }
    
    m_module.opLabel(mergeBlock);
  }
  
  
  void DxbcCompiler::emitControlFlowRet(const DxbcShaderInstruction& ins) {
    m_module.opReturn();
    
    if (m_controlFlowBlocks.size() == 0)
      m_module.functionEnd();
    else
      m_module.opLabel(m_module.allocateId());
  }
  
  
  void DxbcCompiler::emitControlFlowDiscard(const DxbcShaderInstruction& ins) {
    // Discard actually has an operand that determines
    // whether or not the fragment should be discarded
    const DxbcRegisterValue condition = emitRegisterLoad(
      ins.src[0], DxbcRegMask(true, false, false, false));
    
    const DxbcRegisterValue zeroTest = emitRegisterZeroTest(
      condition, ins.controls.zeroTest);
    
    // Insert a Pseudo-'If' block
    const uint32_t discardBlock = m_module.allocateId();
    const uint32_t mergeBlock   = m_module.allocateId();
    
    m_module.opSelectionMerge(mergeBlock,
      spv::SelectionControlMaskNone);
    
    m_module.opBranchConditional(
      zeroTest.id, discardBlock, mergeBlock);
    
    // OpKill terminates the block
    m_module.opLabel(discardBlock);
    m_module.opKill();
    
    m_module.opLabel(mergeBlock);
  }
  
  
  void DxbcCompiler::emitControlFlow(const DxbcShaderInstruction& ins) {
    switch (ins.op) {
      case DxbcOpcode::If:
        return this->emitControlFlowIf(ins);
        
      case DxbcOpcode::Else:
        return this->emitControlFlowElse(ins);
        
      case DxbcOpcode::EndIf:
        return this->emitControlFlowEndIf(ins);
        
      case DxbcOpcode::Switch:
        return this->emitControlFlowSwitch(ins);
        
      case DxbcOpcode::Case:
        return this->emitControlFlowCase(ins);
        
      case DxbcOpcode::Default:
        return this->emitControlFlowDefault(ins);
        
      case DxbcOpcode::EndSwitch:
        return this->emitControlFlowEndSwitch(ins);
        
      case DxbcOpcode::Loop:
        return this->emitControlFlowLoop(ins);
        
      case DxbcOpcode::EndLoop:
        return this->emitControlFlowEndLoop(ins);
        
      case DxbcOpcode::Break:
      case DxbcOpcode::Continue:
        return this->emitControlFlowBreak(ins);
        
      case DxbcOpcode::Breakc:
      case DxbcOpcode::Continuec:
        return this->emitControlFlowBreakc(ins);
        
      case DxbcOpcode::Ret:
        return this->emitControlFlowRet(ins);
        
      case DxbcOpcode::Discard:
        return this->emitControlFlowDiscard(ins);
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled instruction: ",
          ins.op));
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitBuildConstVecf32(
          float                   x,
          float                   y,
          float                   z,
          float                   w,
    const DxbcRegMask&            writeMask) {
    // TODO refactor these functions into one single template
    std::array<uint32_t, 4> ids = { 0, 0, 0, 0 };
    uint32_t componentIndex = 0;
    
    if (writeMask[0]) ids[componentIndex++] = m_module.constf32(x);
    if (writeMask[1]) ids[componentIndex++] = m_module.constf32(y);
    if (writeMask[2]) ids[componentIndex++] = m_module.constf32(z);
    if (writeMask[3]) ids[componentIndex++] = m_module.constf32(w);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Float32;
    result.type.ccount = componentIndex;
    result.id = componentIndex > 1
      ? m_module.constComposite(
          getVectorTypeId(result.type),
          componentIndex, ids.data())
      : ids[0];
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitBuildConstVecu32(
          uint32_t                x,
          uint32_t                y,
          uint32_t                z,
          uint32_t                w,
    const DxbcRegMask&            writeMask) {
    std::array<uint32_t, 4> ids = { 0, 0, 0, 0 };
    uint32_t componentIndex = 0;
    
    if (writeMask[0]) ids[componentIndex++] = m_module.constu32(x);
    if (writeMask[1]) ids[componentIndex++] = m_module.constu32(y);
    if (writeMask[2]) ids[componentIndex++] = m_module.constu32(z);
    if (writeMask[3]) ids[componentIndex++] = m_module.constu32(w);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = componentIndex;
    result.id = componentIndex > 1
      ? m_module.constComposite(
          getVectorTypeId(result.type),
          componentIndex, ids.data())
      : ids[0];
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitBuildConstVeci32(
          int32_t                 x,
          int32_t                 y,
          int32_t                 z,
          int32_t                 w,
    const DxbcRegMask&            writeMask) {
    std::array<uint32_t, 4> ids = { 0, 0, 0, 0 };
    uint32_t componentIndex = 0;
    
    if (writeMask[0]) ids[componentIndex++] = m_module.consti32(x);
    if (writeMask[1]) ids[componentIndex++] = m_module.consti32(y);
    if (writeMask[2]) ids[componentIndex++] = m_module.consti32(z);
    if (writeMask[3]) ids[componentIndex++] = m_module.consti32(w);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Sint32;
    result.type.ccount = componentIndex;
    result.id = componentIndex > 1
      ? m_module.constComposite(
          getVectorTypeId(result.type),
          componentIndex, ids.data())
      : ids[0];
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitBuildZero(
          DxbcScalarType          type) {
    DxbcRegisterValue result;
    result.type.ctype  = type;
    result.type.ccount = 1;
    
    switch (type) {
      case DxbcScalarType::Float32: result.id = m_module.constf32(0.0f); break;
      case DxbcScalarType::Uint32:  result.id = m_module.constu32(0); break;
      case DxbcScalarType::Sint32:  result.id = m_module.consti32(0); break;
      default: throw DxvkError("DxbcCompiler: Invalid scalar type");
    }
    
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitBuildZeroVec(
          DxbcVectorType          type) {
    const DxbcRegisterValue scalar = emitBuildZero(type.ctype);
    
    if (type.ccount == 1)
      return scalar;
    
    const std::array<uint32_t, 4> zeroIds = {{
      scalar.id, scalar.id, scalar.id, scalar.id, 
    }};
    
    DxbcRegisterValue result;
    result.type = type;
    result.id = m_module.constComposite(
      getVectorTypeId(result.type),
      zeroIds.size(), zeroIds.data());
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterBitcast(
          DxbcRegisterValue       srcValue,
          DxbcScalarType          dstType) {
    if (srcValue.type.ctype == dstType)
      return srcValue;
    
    // TODO support 64-bit values
    DxbcRegisterValue result;
    result.type.ctype  = dstType;
    result.type.ccount = srcValue.type.ccount;
    result.id = m_module.opBitcast(
      getVectorTypeId(result.type),
      srcValue.id);
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterSwizzle(
          DxbcRegisterValue       value,
          DxbcRegSwizzle          swizzle,
          DxbcRegMask             writeMask) {
    if (value.type.ccount == 1)
      return emitRegisterExtend(value, writeMask.setCount());
    
    std::array<uint32_t, 4> indices;
    
    uint32_t dstIndex = 0;
    
    for (uint32_t i = 0; i < 4; i++) {
      if (writeMask[i])
        indices[dstIndex++] = swizzle[i];
    }
    
    // If the swizzle combined with the mask can be reduced
    // to a no-op, we don't need to insert any instructions.
    bool isIdentitySwizzle = dstIndex == value.type.ccount;
    
    for (uint32_t i = 0; i < dstIndex && isIdentitySwizzle; i++)
      isIdentitySwizzle &= indices[i] == i;
    
    if (isIdentitySwizzle)
      return value;
    
    // Use OpCompositeExtract if the resulting vector contains
    // only one component, and OpVectorShuffle if it is a vector.
    DxbcRegisterValue result;
    result.type.ctype  = value.type.ctype;
    result.type.ccount = dstIndex;
    
    const uint32_t typeId = getVectorTypeId(result.type);
    
    if (dstIndex == 1) {
      result.id = m_module.opCompositeExtract(
        typeId, value.id, 1, indices.data());
    } else {
      result.id = m_module.opVectorShuffle(
        typeId, value.id, value.id,
        dstIndex, indices.data());
    }
    
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterExtract(
          DxbcRegisterValue       value,
          DxbcRegMask             mask) {
    return emitRegisterSwizzle(value,
      DxbcRegSwizzle(0, 1, 2, 3), mask);
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterInsert(
          DxbcRegisterValue       dstValue,
          DxbcRegisterValue       srcValue,
          DxbcRegMask             srcMask) {
    DxbcRegisterValue result;
    result.type = dstValue.type;
    
    const uint32_t typeId = getVectorTypeId(result.type);
    
    if (srcMask.setCount() == 0) {
      // Nothing to do if the insertion mask is empty
      result.id = dstValue.id;
    } else if (dstValue.type.ccount == 1) {
      // Both values are scalar, so the first component
      // of the write mask decides which one to take.
      result.id = srcMask[0] ? srcValue.id : dstValue.id;
    } else if (srcValue.type.ccount == 1) {
      // The source value is scalar. Since OpVectorShuffle
      // requires both arguments to be vectors, we have to
      // use OpCompositeInsert to modify the vector instead.
      const uint32_t componentId = srcMask.firstSet();
      
      result.id = m_module.opCompositeInsert(typeId,
        srcValue.id, dstValue.id, 1, &componentId);
    } else {
      // Both arguments are vectors. We can determine which
      // components to take from which vector and use the
      // OpVectorShuffle instruction.
      std::array<uint32_t, 4> components;
      uint32_t srcComponentId = dstValue.type.ccount;
      
      for (uint32_t i = 0; i < dstValue.type.ccount; i++)
        components.at(i) = srcMask[i] ? srcComponentId++ : i;
      
      result.id = m_module.opVectorShuffle(
        typeId, dstValue.id, srcValue.id,
        dstValue.type.ccount, components.data());
    }
    
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterExtend(
          DxbcRegisterValue       value,
          uint32_t                size) {
    if (size == 1)
      return value;
    
    std::array<uint32_t, 4> ids = {
      value.id, value.id,
      value.id, value.id, 
    };
    
    DxbcRegisterValue result;
    result.type.ctype  = value.type.ctype;
    result.type.ccount = size;
    result.id = m_module.opCompositeConstruct(
      getVectorTypeId(result.type),
      size, ids.data());
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterAbsolute(
          DxbcRegisterValue       value) {
    const uint32_t typeId = getVectorTypeId(value.type);
    
    switch (value.type.ctype) {
      case DxbcScalarType::Float32: value.id = m_module.opFAbs(typeId, value.id); break;
      case DxbcScalarType::Sint32:  value.id = m_module.opSAbs(typeId, value.id); break;
      default: Logger::warn("DxbcCompiler: Cannot get absolute value for given type");
    }
    
    return value;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterNegate(
          DxbcRegisterValue       value) {
    const uint32_t typeId = getVectorTypeId(value.type);
    
    switch (value.type.ctype) {
      case DxbcScalarType::Float32: value.id = m_module.opFNegate(typeId, value.id); break;
      case DxbcScalarType::Sint32:  value.id = m_module.opSNegate(typeId, value.id); break;
      default: Logger::warn("DxbcCompiler: Cannot negate given type");
    }
    
    return value;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterZeroTest(
          DxbcRegisterValue       value,
          DxbcZeroTest            test) {
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Bool;
    result.type.ccount = 1;
    
    const uint32_t zeroId = m_module.constu32(0u);
    const uint32_t typeId = getVectorTypeId(result.type);
    
    result.id = test == DxbcZeroTest::TestZ
      ? m_module.opIEqual   (typeId, value.id, zeroId)
      : m_module.opINotEqual(typeId, value.id, zeroId);
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitSrcOperandModifiers(
          DxbcRegisterValue       value,
          DxbcRegModifiers        modifiers) {
    if (modifiers.test(DxbcRegModifier::Abs))
      value = emitRegisterAbsolute(value);
    
    if (modifiers.test(DxbcRegModifier::Neg))
      value = emitRegisterNegate(value);
    return value;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitDstOperandModifiers(
          DxbcRegisterValue       value,
          DxbcOpModifiers         modifiers) {
    const uint32_t typeId = getVectorTypeId(value.type);
    
    if (value.type.ctype == DxbcScalarType::Float32) {
      // Saturating only makes sense on floats
      if (modifiers.saturate) {
        const DxbcRegMask       mask = DxbcRegMask::firstN(value.type.ccount);
        const DxbcRegisterValue vec0 = emitBuildConstVecf32(0.0f, 0.0f, 0.0f, 0.0f, mask);
        const DxbcRegisterValue vec1 = emitBuildConstVecf32(1.0f, 1.0f, 1.0f, 1.0f, mask);
        
        value.id = m_options.useSimpleMinMaxClamp
          ? m_module.opFClamp(typeId, value.id, vec0.id, vec1.id)
          : m_module.opNClamp(typeId, value.id, vec0.id, vec1.id);
      }
    }
    
    return value;
  }
  
  
  uint32_t DxbcCompiler::emitLoadSampledImage(
    const DxbcShaderResource&     textureResource,
    const DxbcSampler&            samplerResource,
          bool                    isDepthCompare) {
    const uint32_t sampledImageType = isDepthCompare
      ? m_module.defSampledImageType(textureResource.depthTypeId)
      : m_module.defSampledImageType(textureResource.colorTypeId);
    
    return m_module.opSampledImage(sampledImageType,
      m_module.opLoad(textureResource.imageTypeId, textureResource.varId),
      m_module.opLoad(samplerResource.typeId,      samplerResource.varId));
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetTempPtr(
    const DxbcRegister&           operand) {
    // r# regs are indexed as follows:
    //    (0) register index (immediate)
    DxbcRegisterPointer result;
    result.type.ctype  = DxbcScalarType::Float32;
    result.type.ccount = 4;
    result.id = m_rRegs.at(operand.idx[0].offset);
    return result;
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetIndexableTempPtr(
    const DxbcRegister&           operand) {
    // x# regs are indexed as follows:
    //    (0) register index (immediate)
    //    (1) element index (relative)
    const uint32_t regId = operand.idx[0].offset;
    
    const DxbcRegisterValue vectorId
      = emitIndexLoad(operand.idx[1]);
    
    DxbcRegisterInfo info;
    info.type.ctype   = DxbcScalarType::Float32;
    info.type.ccount  = m_xRegs[regId].ccount;
    info.type.alength = 0;
    info.sclass       = spv::StorageClassPrivate;
    
    DxbcRegisterPointer result;
    result.type.ctype  = info.type.ctype;
    result.type.ccount = info.type.ccount;
    result.id = m_module.opAccessChain(
      getPointerTypeId(info),
      m_xRegs.at(regId).varId,
      1, &vectorId.id);
    return result;
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetInputPtr(
    const DxbcRegister&           operand) {
    // In the vertex and pixel stages,
    // v# regs are indexed as follows:
    //    (0) register index (relative)
    // 
    // In the tessellation and geometry
    // stages, the index has two dimensions:
    //    (0) vertex index (relative)
    //    (1) register index (relative)
    DxbcRegisterPointer result;
    result.type.ctype  = DxbcScalarType::Float32;
    result.type.ccount = 4;
    
    std::array<uint32_t, 2> indices = { 0, 0 };
    
    for (uint32_t i = 0; i < operand.idxDim; i++)
      indices.at(i) = emitIndexLoad(operand.idx[i]).id;
      
    DxbcRegisterInfo info;
    info.type.ctype   = result.type.ctype;
    info.type.ccount  = result.type.ccount;
    info.type.alength = 0;
    info.sclass = spv::StorageClassPrivate;
      
    result.id = m_module.opAccessChain(
      getPointerTypeId(info), m_vArray,
      operand.idxDim, indices.data());
    
    return result;
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetOutputPtr(
    const DxbcRegister&           operand) {
    // Same index format as input registers, except that
    // outputs cannot be accessed with a relative index.
    if (operand.idxDim != 1)
      throw DxvkError("DxbcCompiler: 2D index for o# not yet supported");
    
    // We don't support two-dimensional indices yet
    const uint32_t registerId = operand.idx[0].offset;
    
    // In the pixel shader, output registers are typed,
    // whereas they are float4 in all other stages.
    if (m_version.type() == DxbcProgramType::PixelShader) {
      DxbcRegisterPointer result;
      result.type = m_ps.oTypes.at(registerId);
      result.id = m_oRegs.at(registerId);
      return result;
    } else {
      DxbcRegisterPointer result;
      result.type.ctype  = DxbcScalarType::Float32;
      result.type.ccount = 4;
      result.id = m_oRegs.at(registerId);
      return result;
    }
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetConstBufPtr(
    const DxbcRegister&           operand) {
    // Constant buffers take a two-dimensional index:
    //    (0) register index (immediate)
    //    (1) constant offset (relative)
    DxbcRegisterInfo info;
    info.type.ctype   = DxbcScalarType::Float32;
    info.type.ccount  = 4;
    info.type.alength = 0;
    info.sclass = spv::StorageClassUniform;
    
    const uint32_t regId = operand.idx[0].offset;
    const DxbcRegisterValue constId = emitIndexLoad(operand.idx[1]);
    
    const uint32_t ptrTypeId = getPointerTypeId(info);
    
    const std::array<uint32_t, 2> indices =
      {{ m_module.consti32(0), constId.id }};
    
    DxbcRegisterPointer result;
    result.type.ctype  = info.type.ctype;
    result.type.ccount = info.type.ccount;
    result.id = m_module.opAccessChain(ptrTypeId,
      m_constantBuffers.at(regId).varId,
      indices.size(), indices.data());
    return result;
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetImmConstBufPtr(
    const DxbcRegister&           operand) {
    if (m_immConstBuf == 0)
      throw DxvkError("DxbcCompiler: Immediate constant buffer not defined");
    
    const DxbcRegisterValue constId
      = emitIndexLoad(operand.idx[0]);
    
    DxbcRegisterInfo ptrInfo;
    ptrInfo.type.ctype   = DxbcScalarType::Uint32;
    ptrInfo.type.ccount  = 4;
    ptrInfo.type.alength = 0;
    ptrInfo.sclass = spv::StorageClassPrivate;
    
    DxbcRegisterPointer result;
    result.type.ctype  = ptrInfo.type.ctype;
    result.type.ccount = ptrInfo.type.ccount;
    result.id = m_module.opAccessChain(
      getPointerTypeId(ptrInfo),
      m_immConstBuf, 1, &constId.id);
    return result;
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetOperandPtr(
    const DxbcRegister&           operand) {
    switch (operand.type) {
      case DxbcOperandType::Temp:
        return emitGetTempPtr(operand);
      
      case DxbcOperandType::IndexableTemp:
        return emitGetIndexableTempPtr(operand);
      
      case DxbcOperandType::Input:
        return emitGetInputPtr(operand);
      
      case DxbcOperandType::Output:
        return emitGetOutputPtr(operand);
      
      case DxbcOperandType::ConstantBuffer:
        return emitGetConstBufPtr(operand);
      
      case DxbcOperandType::ImmediateConstantBuffer:
        return emitGetImmConstBufPtr(operand);
      
      case DxbcOperandType::InputThreadId:
        return DxbcRegisterPointer {
          { DxbcScalarType::Uint32, 3 },
          m_cs.builtinGlobalInvocationId };
      
      case DxbcOperandType::InputThreadGroupId:
        return DxbcRegisterPointer {
          { DxbcScalarType::Uint32, 3 },
          m_cs.builtinWorkgroupId };
      
      case DxbcOperandType::InputThreadIdInGroup:
        return DxbcRegisterPointer {
          { DxbcScalarType::Uint32, 3 },
          m_cs.builtinLocalInvocationId };
      
      case DxbcOperandType::InputThreadIndexInGroup:
        return DxbcRegisterPointer {
          { DxbcScalarType::Uint32, 1 },
          m_cs.builtinLocalInvocationIndex };
      
      case DxbcOperandType::InputCoverageMask: {
        const std::array<uint32_t, 1> indices
          = {{ m_module.constu32(0) }};
        
        DxbcRegisterPointer result;
        result.type.ctype  = DxbcScalarType::Uint32;
        result.type.ccount = 1;
        result.id = m_module.opAccessChain(
          m_module.defPointerType(
            getVectorTypeId(result.type),
            spv::StorageClassInput),
          m_ps.builtinSampleMaskIn,
          indices.size(), indices.data());
        return result;
      }
        
      case DxbcOperandType::OutputCoverageMask: {
        const std::array<uint32_t, 1> indices
          = {{ m_module.constu32(0) }};
        
        DxbcRegisterPointer result;
        result.type.ctype  = DxbcScalarType::Uint32;
        result.type.ccount = 1;
        result.id = m_module.opAccessChain(
          m_module.defPointerType(
            getVectorTypeId(result.type),
            spv::StorageClassOutput),
          m_ps.builtinSampleMaskOut,
          indices.size(), indices.data());
        return result;
      }
        
      case DxbcOperandType::OutputDepth:
      case DxbcOperandType::OutputDepthGe:
      case DxbcOperandType::OutputDepthLe:
        return DxbcRegisterPointer {
          { DxbcScalarType::Float32, 1 },
          m_ps.builtinDepth };
        
      default:
        throw DxvkError(str::format(
          "DxbcCompiler: Unhandled operand type: ",
          operand.type));
    }
  }
  
  
  DxbcRegisterPointer DxbcCompiler::emitGetAtomicPointer(
    const DxbcRegister&           operand,
    const DxbcRegister&           address) {
    // Query information about the resource itself
    const uint32_t registerId = operand.idx[0].offset;
    const DxbcBufferInfo resourceInfo = getBufferInfo(operand);
    
    // For UAVs and shared memory, different methods
    // of obtaining the final pointer are used.
    const bool isUav = operand.type == DxbcOperandType::UnorderedAccessView;
    
    // Compute the actual address into the resource
    const DxbcRegisterValue addressValue = [&] {
      switch (resourceInfo.type) {
        case DxbcResourceType::Raw:
          return emitCalcBufferIndexRaw(emitRegisterLoad(
            address, DxbcRegMask(true, false, false, false)));
          
        case DxbcResourceType::Structured: {
          const DxbcRegisterValue addressComponents = emitRegisterLoad(
            address, DxbcRegMask(true, true, false, false));
          
          return emitCalcBufferIndexStructured(
            emitRegisterExtract(addressComponents, DxbcRegMask(true, false, false, false)),
            emitRegisterExtract(addressComponents, DxbcRegMask(false, true, false, false)),
            resourceInfo.stride);
        };
        
        case DxbcResourceType::Typed: {
          if (!isUav)
            throw DxvkError("DxbcCompiler: TGSM cannot be typed");
          
          return emitRegisterLoad(address, getTexCoordMask(
            m_uavs.at(registerId).imageInfo));
        }
        
        default:
          throw DxvkError("DxbcCompiler: Unhandled resource type");
      }
    }();
    
    // Compute the actual pointer
    DxbcRegisterPointer result;
    result.type.ctype  = operand.dataType;
    result.type.ccount = 1;
    result.id = isUav
      ? m_module.opImageTexelPointer(
          m_module.defPointerType(getVectorTypeId(result.type), spv::StorageClassImage),
          m_uavs.at(registerId).varId, addressValue.id, m_module.constu32(0))
      : m_module.opAccessChain(
          m_module.defPointerType(getVectorTypeId(result.type), spv::StorageClassWorkgroup),
          m_gRegs.at(registerId).varId, 1, &addressValue.id);
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRawBufferLoad(
    const DxbcRegister&           operand,
          DxbcRegisterValue       elementIndex,
          DxbcRegMask             writeMask) {
    const DxbcBufferInfo bufferInfo = getBufferInfo(operand);
    
    // Shared memory is the only type of buffer that
    // is not accessed through a texel buffer view
    const bool isTgsm = operand.type == DxbcOperandType::ThreadGroupSharedMemory;
    
    const uint32_t bufferId = isTgsm
      ? 0 : m_module.opLoad(bufferInfo.typeId, bufferInfo.varId);
    
    // Since all data is represented as a sequence of 32-bit
    // integers, we have to load each component individually.
    std::array<uint32_t, 4> componentIds = { 0, 0, 0, 0 };
    std::array<uint32_t, 4> swizzleIds   = { 0, 0, 0, 0 };
    
    uint32_t componentIndex = 0;
    
    const uint32_t vectorTypeId = getVectorTypeId({ DxbcScalarType::Uint32, 4 });
    const uint32_t scalarTypeId = getVectorTypeId({ DxbcScalarType::Uint32, 1 });
    
    for (uint32_t i = 0; i < 4; i++) {
      // We'll apply both the write mask and the source operand swizzle
      // immediately. Unused components are not loaded, and the scalar
      // IDs are written to the array in the order they are requested.
      if (writeMask[i]) {
        const uint32_t swizzleIndex = operand.swizzle[i];
        
        if (componentIds[swizzleIndex] == 0) {
          // Add the component offset to the element index
          const uint32_t elementIndexAdjusted = m_module.opIAdd(
            getVectorTypeId(elementIndex.type), elementIndex.id,
            m_module.consti32(swizzleIndex));
          
          // Load requested component from the buffer
          componentIds[swizzleIndex] = [&] {
            const uint32_t zero = 0;
            
            switch (operand.type) {
              case DxbcOperandType::Resource:
                return m_module.opCompositeExtract(scalarTypeId,
                  m_module.opImageFetch(vectorTypeId,
                    bufferId, elementIndexAdjusted,
                    SpirvImageOperands()), 1, &zero);
              
              case DxbcOperandType::UnorderedAccessView:
                return m_module.opCompositeExtract(scalarTypeId,
                  m_module.opImageRead(vectorTypeId,
                    bufferId, elementIndexAdjusted,
                    SpirvImageOperands()), 1, &zero);
              
              case DxbcOperandType::ThreadGroupSharedMemory:
                return m_module.opLoad(scalarTypeId,
                  m_module.opAccessChain(bufferInfo.typeId,
                    bufferInfo.varId, 1, &elementIndexAdjusted));
                
              default:
                throw DxvkError("DxbcCompiler: Invalid operand type for strucured/raw load");
            }
          }();
        }
        
        // Append current component to the list of scalar IDs.
        // These will be used to construct the resulting vector.
        swizzleIds[componentIndex++] = componentIds[swizzleIndex];
      }
    }
    
    // Create result vector
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = writeMask.setCount();
    result.id = result.type.ccount > 1
      ? m_module.opCompositeConstruct(getVectorTypeId(result.type),
          result.type.ccount, swizzleIds.data())
      : swizzleIds[0];
    return result;
  }
  
  
  void DxbcCompiler::emitRawBufferStore(
    const DxbcRegister&           operand,
          DxbcRegisterValue       elementIndex,
          DxbcRegisterValue       value) {
    const DxbcBufferInfo bufferInfo = getBufferInfo(operand);
    
    // Cast source value to the expected data type
    value = emitRegisterBitcast(value, DxbcScalarType::Uint32);
    
    // Shared memory is not accessed through a texel buffer view
    const bool isTgsm = operand.type == DxbcOperandType::ThreadGroupSharedMemory;
    
    const uint32_t bufferId = isTgsm
      ? 0 : m_module.opLoad(bufferInfo.typeId, bufferInfo.varId);
    
    const uint32_t scalarTypeId = getVectorTypeId({ DxbcScalarType::Uint32, 1 });
    const uint32_t vectorTypeId = getVectorTypeId({ DxbcScalarType::Uint32, 4 });
    
    uint32_t srcComponentIndex = 0;
    
    for (uint32_t i = 0; i < 4; i++) {
      if (operand.mask[i]) {
        const uint32_t srcComponentId = value.type.ccount > 1
          ? m_module.opCompositeExtract(scalarTypeId,
              value.id, 1, &srcComponentIndex)
          : value.id;
        
        // Add the component offset to the element index
        const uint32_t elementIndexAdjusted = i != 0
          ? m_module.opIAdd(getVectorTypeId(elementIndex.type),
              elementIndex.id, m_module.consti32(i))
          : elementIndex.id;
        
        switch (operand.type) {
          case DxbcOperandType::UnorderedAccessView: {
            const std::array<uint32_t, 4> srcVectorIds = {
              srcComponentId, srcComponentId,
              srcComponentId, srcComponentId,
            };
        
            m_module.opImageWrite(
              bufferId, elementIndexAdjusted,
              m_module.opCompositeConstruct(vectorTypeId,
                4, srcVectorIds.data()),
              SpirvImageOperands());
          } break;
          
          case DxbcOperandType::ThreadGroupSharedMemory:
            m_module.opStore(
              m_module.opAccessChain(bufferInfo.typeId,
                bufferInfo.varId, 1, &elementIndexAdjusted),
              srcComponentId);
            break;
          
          default:
            throw DxvkError("DxbcCompiler: Invalid operand type for strucured/raw store");
        }
        
        // Write next component
        srcComponentIndex += 1;
      }
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitQueryTexelBufferSize(
    const DxbcRegister&           resource) {
    // Load the texel buffer object. This cannot be used with
    // constant buffers or any other type of resource.
    const DxbcBufferInfo bufferInfo = getBufferInfo(resource);
    
    const uint32_t bufferId = m_module.opLoad(
      bufferInfo.typeId, bufferInfo.varId);
    
    // We'll store this as a scalar unsigned integer
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = 1;
    result.id = m_module.opImageQuerySize(
      getVectorTypeId(result.type), bufferId);
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitQueryTextureLods(
    const DxbcRegister&           resource) {
    const DxbcBufferInfo info = getBufferInfo(resource);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = 1;
    
    if (info.image.sampled == 1) {
      result.id = m_module.opImageQueryLevels(
        getVectorTypeId(result.type),
        m_module.opLoad(info.typeId, info.varId));
    } else {
      // Report one LOD in case of UAVs
      result.id = m_module.constu32(1);
    }
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitQueryTextureSamples(
    const DxbcRegister&           resource) {
    const DxbcBufferInfo info = getBufferInfo(resource);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = 1;
    result.id = m_module.opImageQuerySamples(
      getVectorTypeId(result.type),
      m_module.opLoad(info.typeId, info.varId));
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitQueryTextureSize(
    const DxbcRegister&           resource,
          DxbcRegisterValue       lod) {
    const DxbcBufferInfo info = getBufferInfo(resource);
    
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Uint32;
    result.type.ccount = getTexCoordDim(info.image);
    
    if (info.image.ms == 0 && info.image.sampled == 1) {
      result.id = m_module.opImageQuerySizeLod(
        getVectorTypeId(result.type),
        m_module.opLoad(info.typeId, info.varId),
        lod.id);
    } else {
      result.id = m_module.opImageQuerySize(
        getVectorTypeId(result.type),
        m_module.opLoad(info.typeId, info.varId));
    }
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitCalcBufferIndexStructured(
          DxbcRegisterValue       structId,
          DxbcRegisterValue       structOffset,
          uint32_t                structStride) {
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Sint32;
    result.type.ccount = 1;
    
    const uint32_t typeId = getVectorTypeId(result.type);
    
    result.id = m_module.opIAdd(typeId,
      m_module.opIMul(typeId, structId.id, m_module.consti32(structStride / 4)),
      m_module.opSDiv(typeId, structOffset.id, m_module.consti32(4)));
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitCalcBufferIndexRaw(
          DxbcRegisterValue       byteOffset) {
    DxbcRegisterValue result;
    result.type.ctype  = DxbcScalarType::Sint32;
    result.type.ccount = 1;
    result.id = m_module.opSDiv(
      getVectorTypeId(result.type),
      byteOffset.id,
      m_module.consti32(4));
    return result;
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitIndexLoad(
          DxbcRegIndex            index) {
    if (index.relReg != nullptr) {
      DxbcRegisterValue result = emitRegisterLoad(
        *index.relReg, DxbcRegMask(true, false, false, false));
      
      if (index.offset != 0) {
        result.id = m_module.opIAdd(
          getVectorTypeId(result.type), result.id,
          m_module.consti32(index.offset));
      }
      
      return result;
    } else {
      DxbcRegisterValue result;
      result.type.ctype  = DxbcScalarType::Sint32;
      result.type.ccount = 1;
      result.id = m_module.consti32(index.offset);
      return result;
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitValueLoad(
          DxbcRegisterPointer     ptr) {
    DxbcRegisterValue result;
    result.type = ptr.type;
    result.id   = m_module.opLoad(
      getVectorTypeId(result.type),
      ptr.id);
    return result;
  }
  
  
  void DxbcCompiler::emitValueStore(
          DxbcRegisterPointer     ptr,
          DxbcRegisterValue       value,
          DxbcRegMask             writeMask) {
    // If the component types are not compatible,
    // we need to bit-cast the source variable.
    if (value.type.ctype != ptr.type.ctype)
      value = emitRegisterBitcast(value, ptr.type.ctype);
    
    // If the source value consists of only one component,
    // it is stored in all components of the destination.
    if (value.type.ccount == 1)
      value = emitRegisterExtend(value, writeMask.setCount());
    
    if (ptr.type.ccount == writeMask.setCount()) {
      // Simple case: We write to the entire register
      m_module.opStore(ptr.id, value.id);
    } else {
      // We only write to part of the destination
      // register, so we need to load and modify it
      DxbcRegisterValue tmp = emitValueLoad(ptr);
      tmp = emitRegisterInsert(tmp, value, writeMask);
      
      m_module.opStore(ptr.id, tmp.id);
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterLoadRaw(
    const DxbcRegister&           reg) {
    if (reg.type == DxbcOperandType::ConstantBuffer) {
      // Constant buffer require special care if they are not bound
      const uint32_t registerId = reg.idx[0].offset;
      
      const uint32_t labelMerge   = m_module.allocateId();
      const uint32_t labelBound   = m_module.allocateId();
      const uint32_t labelUnbound = m_module.allocateId();
      
      m_module.opSelectionMerge(labelMerge, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(
        m_constantBuffers.at(registerId).specId,
        labelBound, labelUnbound);
      
      // Case 1: Constant buffer is bound.
      // Load the register value normally.
      m_module.opLabel(labelBound);
      DxbcRegisterValue ifBound = emitValueLoad(emitGetOperandPtr(reg));
      m_module.opBranch(labelMerge);
      
      // Case 2: Constant buffer is not bound.
      // Return zeroes unconditionally.
      m_module.opLabel(labelUnbound);
      DxbcRegisterValue ifUnbound = emitBuildConstVecf32(
        0.0f, 0.0f, 0.0f, 0.0f,
        DxbcRegMask(true, true, true, true));
      m_module.opBranch(labelMerge);
      
      // Merge the results with a phi function
      m_module.opLabel(labelMerge);
      
      const std::array<SpirvPhiLabel, 2> phiLabels = {{
        { ifBound.id,   labelBound   },
        { ifUnbound.id, labelUnbound },
      }};
      
      DxbcRegisterValue result;
      result.type.ctype  = DxbcScalarType::Float32;
      result.type.ccount = 4;
      result.id = m_module.opPhi(
        getVectorTypeId(result.type),
        phiLabels.size(),
        phiLabels.data());
      return result;
    } else {
      // All other operand types can be accessed directly
      return emitValueLoad(emitGetOperandPtr(reg));
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitRegisterLoad(
    const DxbcRegister&           reg,
          DxbcRegMask             writeMask) {
    if (reg.type == DxbcOperandType::Imm32) {
      DxbcRegisterValue result;
      
      if (reg.componentCount == DxbcComponentCount::Component1) {
        // Create one single u32 constant
        result.type.ctype  = DxbcScalarType::Uint32;
        result.type.ccount = 1;
        result.id = m_module.constu32(reg.imm.u32_1);
      } else if (reg.componentCount == DxbcComponentCount::Component4) {
        // Create a u32 vector with as many components as needed
        std::array<uint32_t, 4> indices;
        uint32_t indexId = 0;
        
        for (uint32_t i = 0; i < indices.size(); i++) {
          if (writeMask[i]) {
            indices.at(indexId++) =
              m_module.constu32(reg.imm.u32_4[i]);
          }
        }
        
        result.type.ctype  = DxbcScalarType::Uint32;
        result.type.ccount = writeMask.setCount();
        result.id = indices.at(0);
        
        if (indexId > 1) {
          result.id = m_module.constComposite(
            getVectorTypeId(result.type),
            result.type.ccount, indices.data());
        }
        
      } else {
        // Something went horribly wrong in the decoder or the shader is broken
        throw DxvkError("DxbcCompiler: Invalid component count for immediate operand");
      }
      
      // Cast constants to the requested type
      return emitRegisterBitcast(result, reg.dataType);
    } else {
      // Load operand from the operand pointer
      DxbcRegisterValue result = emitRegisterLoadRaw(reg);
      
      // Apply operand swizzle to the operand value
      result = emitRegisterSwizzle(result, reg.swizzle, writeMask);
      
      // Cast it to the requested type. We need to do
      // this after the swizzling for 64-bit types.
      result = emitRegisterBitcast(result, reg.dataType);
      
      // Apply operand modifiers
      result = emitSrcOperandModifiers(result, reg.modifiers);
      return result;
    }
  }
  
  
  void DxbcCompiler::emitRegisterStore(
    const DxbcRegister&           reg,
          DxbcRegisterValue       value) {
    emitValueStore(emitGetOperandPtr(reg), value, reg.mask);
  }
  
  
  void DxbcCompiler::emitInputSetup() {
    // Copy all defined v# registers into the input array
    const uint32_t vecTypeId = m_module.defVectorType(m_module.defFloatType(32), 4);
    const uint32_t ptrTypeId = m_module.defPointerType(vecTypeId, spv::StorageClassPrivate);
    
    for (uint32_t i = 0; i < m_vRegs.size(); i++) {
      if (m_vRegs.at(i) != 0) {
        const uint32_t registerId = m_module.consti32(i);
        const uint32_t srcTypeId = getVectorTypeId(getInputRegType(i));
        const uint32_t srcValue  = m_module.opLoad(srcTypeId, m_vRegs.at(i));
        
        m_module.opStore(m_module.opAccessChain(ptrTypeId, m_vArray, 1, &registerId),
          vecTypeId != srcTypeId ? m_module.opBitcast(vecTypeId, srcValue) : srcValue);
      }
    }
    
    // Copy all system value registers into the array,
    // preserving any previously written contents.
    for (const DxbcSvMapping& map : m_vMappings) {
      const uint32_t registerId = m_module.consti32(map.regId);
      
      const DxbcRegisterValue value = [&] {
        switch (m_version.type()) {
          case DxbcProgramType::VertexShader:   return emitVsSystemValueLoad(map.sv, map.regMask);
          case DxbcProgramType::PixelShader:    return emitPsSystemValueLoad(map.sv, map.regMask);
          case DxbcProgramType::ComputeShader:  return emitCsSystemValueLoad(map.sv, map.regMask);
          default: throw DxvkError(str::format("DxbcCompiler: Unexpected stage: ", m_version.type()));
        }
      }();
      
      DxbcRegisterPointer inputReg;
      inputReg.type.ctype  = DxbcScalarType::Float32;
      inputReg.type.ccount = 4;
      inputReg.id = m_module.opAccessChain(
        ptrTypeId, m_vArray, 1, &registerId);
      emitValueStore(inputReg, value, map.regMask);
    }
  }
  
  
  void DxbcCompiler::emitInputSetup(uint32_t vertexCount) {
    // Copy all defined v# registers into the input array. Note
    // that the outer index of the array is the vertex index.
    const uint32_t vecTypeId    = m_module.defVectorType(m_module.defFloatType(32), 4);
    const uint32_t dstPtrTypeId = m_module.defPointerType(vecTypeId, spv::StorageClassPrivate);
    const uint32_t srcPtrTypeId = m_module.defPointerType(vecTypeId, spv::StorageClassInput);
    
    for (uint32_t i = 0; i < m_vRegs.size(); i++) {
      if (m_vRegs.at(i) != 0) {
        const uint32_t registerId = m_module.consti32(i);
        
        for (uint32_t v = 0; v < vertexCount; v++) {
          std::array<uint32_t, 2> indices
            = {{ m_module.consti32(v), registerId }};
          
          const uint32_t srcTypeId = getVectorTypeId(getInputRegType(i));
          const uint32_t srcValue  = m_module.opLoad(srcTypeId,
            m_module.opAccessChain(srcPtrTypeId, m_vRegs.at(i), 1, indices.data()));
          
          m_module.opStore(
            m_module.opAccessChain(dstPtrTypeId, m_vArray, indices.size(), indices.data()),
            vecTypeId != srcTypeId ? m_module.opBitcast(vecTypeId, srcValue) : srcValue);
        }
      }
    }
    
    // Copy all system value registers into the array,
    // preserving any previously written contents.
    for (const DxbcSvMapping& map : m_vMappings) {
      const uint32_t registerId = m_module.consti32(map.regId);
      
      for (uint32_t v = 0; v < vertexCount; v++) {
        const DxbcRegisterValue value = [&] {
          switch (m_version.type()) {
            case DxbcProgramType::GeometryShader: return emitGsSystemValueLoad(map.sv, map.regMask, v);
            default: throw DxvkError(str::format("DxbcCompiler: Unexpected stage: ", m_version.type()));
          }
        }();
        
        std::array<uint32_t, 2> indices = {
          m_module.consti32(v), registerId,
        };
        
        DxbcRegisterPointer inputReg;
        inputReg.type.ctype  = DxbcScalarType::Float32;
        inputReg.type.ccount = 4;
        inputReg.id = m_module.opAccessChain(dstPtrTypeId,
          m_vArray, indices.size(), indices.data());
        emitValueStore(inputReg, value, map.regMask);
      }
    }
  }
  
  
  void DxbcCompiler::emitOutputSetup() {
    for (const DxbcSvMapping& svMapping : m_oMappings) {
      DxbcRegisterPointer outputReg;
      outputReg.type.ctype  = DxbcScalarType::Float32;
      outputReg.type.ccount = 4;
      outputReg.id = m_oRegs.at(svMapping.regId);
      
      auto sv    = svMapping.sv;
      auto mask  = svMapping.regMask;
      auto value = emitValueLoad(outputReg);
      
      switch (m_version.type()) {
        case DxbcProgramType::VertexShader:   emitVsSystemValueStore(sv, mask, value); break;
        case DxbcProgramType::GeometryShader: emitGsSystemValueStore(sv, mask, value); break;
        case DxbcProgramType::HullShader:
        case DxbcProgramType::DomainShader:
        case DxbcProgramType::PixelShader:
        case DxbcProgramType::ComputeShader:
          break;
      }
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitVsSystemValueLoad(
          DxbcSystemValue         sv,
          DxbcRegMask             mask) {
    switch (sv) {
      case DxbcSystemValue::VertexId: {
        const uint32_t typeId = getScalarTypeId(DxbcScalarType::Uint32);
        
        if (m_vs.builtinVertexId == 0) {
          m_vs.builtinVertexId = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInVertexIndex,
            "vs_vertex_index");
        }
        
        if (m_vs.builtinBaseVertex == 0) {
          m_vs.builtinBaseVertex = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInBaseVertex,
            "vs_base_vertex");
        }
        
        DxbcRegisterValue result;
        result.type.ctype   = DxbcScalarType::Uint32;
        result.type.ccount  = 1;
        result.id = m_module.opISub(typeId,
          m_module.opLoad(typeId, m_vs.builtinVertexId),
          m_module.opLoad(typeId, m_vs.builtinBaseVertex));
        return result;
      } break;
      
      case DxbcSystemValue::InstanceId: {
        const uint32_t typeId = getScalarTypeId(DxbcScalarType::Uint32);
        
        if (m_vs.builtinInstanceId == 0) {
          m_vs.builtinInstanceId = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInInstanceIndex,
            "vs_instance_index");
        }
          
        if (m_vs.builtinBaseInstance == 0) {
          m_vs.builtinBaseInstance = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInBaseInstance,
            "vs_base_instance");
        }
        
        DxbcRegisterValue result;
        result.type.ctype   = DxbcScalarType::Uint32;
        result.type.ccount  = 1;
        result.id = m_module.opISub(typeId,
          m_module.opLoad(typeId, m_vs.builtinInstanceId),
          m_module.opLoad(typeId, m_vs.builtinBaseInstance));
        return result;
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler: Unhandled VS SV input: ", sv));
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitGsSystemValueLoad(
          DxbcSystemValue         sv,
          DxbcRegMask             mask,
          uint32_t                vertexId) {
    switch (sv) {
      case DxbcSystemValue::Position: {
        const std::array<uint32_t, 2> indices = {
          m_module.consti32(vertexId),
          m_module.consti32(PerVertex_Position),
        };
        
        DxbcRegisterPointer ptrIn;
        ptrIn.type.ctype  = DxbcScalarType::Float32;
        ptrIn.type.ccount = 4;
        
        ptrIn.id = m_module.opAccessChain(
          m_module.defPointerType(
            getVectorTypeId(ptrIn.type),
            spv::StorageClassInput),
          m_perVertexIn,
          indices.size(),
          indices.data());
        
        return emitRegisterExtract(
          emitValueLoad(ptrIn), mask);
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler: Unhandled GS SV input: ", sv));
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitPsSystemValueLoad(
          DxbcSystemValue         sv,
          DxbcRegMask             mask) {
    switch (sv) {
      case DxbcSystemValue::Position: {
        if (m_ps.builtinFragCoord == 0) {
          m_ps.builtinFragCoord = emitNewBuiltinVariable({
            { DxbcScalarType::Float32, 4, 0 },
            spv::StorageClassInput },
            spv::BuiltInFragCoord,
            "ps_frag_coord");
        }
        
        DxbcRegisterPointer ptrIn;
        ptrIn.type.ctype   = DxbcScalarType::Float32;
        ptrIn.type.ccount  = 4;
        ptrIn.id = m_ps.builtinFragCoord;
        
        return emitRegisterExtract(
          emitValueLoad(ptrIn), mask);
      } break;
      
      case DxbcSystemValue::IsFrontFace: {
        if (m_ps.builtinIsFrontFace == 0) {
          m_ps.builtinIsFrontFace = emitNewBuiltinVariable({
            { DxbcScalarType::Bool, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInFrontFacing,
            "ps_is_front_face");
        }
        
        DxbcRegisterValue result;
        result.type.ctype  = DxbcScalarType::Uint32;
        result.type.ccount = 1;
        result.id = m_module.opSelect(
          getVectorTypeId(result.type),
          m_module.opLoad(
            m_module.defBoolType(),
            m_ps.builtinIsFrontFace),
          m_module.constu32(0xFFFFFFFF),
          m_module.constu32(0x00000000));
        return result;
      } break;
      
      case DxbcSystemValue::SampleIndex: {
        if (m_ps.builtinSampleId == 0) {
          m_module.enableCapability(spv::CapabilitySampleRateShading);
          
          m_ps.builtinSampleId = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInSampleId,
            "ps_sample_id");
        }
        
        DxbcRegisterPointer ptrIn;
        ptrIn.type.ctype   = DxbcScalarType::Uint32;
        ptrIn.type.ccount  = 1;
        ptrIn.id = m_ps.builtinSampleId;
        
        return emitValueLoad(ptrIn);
      } break;
      
      case DxbcSystemValue::RenderTargetId: {
        if (m_ps.builtinLayer == 0) {
          m_module.enableCapability(spv::CapabilityGeometry);
          
          m_ps.builtinLayer = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassInput },
            spv::BuiltInLayer,
            "ps_layer");
        }
        
        DxbcRegisterPointer ptr;
        ptr.type.ctype   = DxbcScalarType::Uint32;
        ptr.type.ccount  = 1;
        ptr.id = m_ps.builtinLayer;
        
        return emitValueLoad(ptr);
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcCompiler: Unhandled PS SV input: ", sv));
    }
  }
  
  
  DxbcRegisterValue DxbcCompiler::emitCsSystemValueLoad(
          DxbcSystemValue         sv,
          DxbcRegMask             mask) {
    switch (sv) {
      default:
        throw DxvkError(str::format(
          "DxbcCompiler: Unhandled CS SV input: ", sv));
    }
  }
  
  
  void DxbcCompiler::emitVsSystemValueStore(
          DxbcSystemValue         sv,
          DxbcRegMask             mask,
    const DxbcRegisterValue&      value) {
    switch (sv) {
      case DxbcSystemValue::Position: {
        const uint32_t memberId = m_module.consti32(PerVertex_Position);
        
        DxbcRegisterPointer ptr;
        ptr.type.ctype  = DxbcScalarType::Float32;
        ptr.type.ccount = 4;
        
        ptr.id = m_module.opAccessChain(
          m_module.defPointerType(
            getVectorTypeId(ptr.type),
            spv::StorageClassOutput),
          m_perVertexOut, 1, &memberId);
        
        emitValueStore(ptr, value, mask);
      } break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled VS SV output: ", sv));
    }
  }
  
  
  void DxbcCompiler::emitGsSystemValueStore(
          DxbcSystemValue         sv,
          DxbcRegMask             mask,
    const DxbcRegisterValue&      value) {
    switch (sv) {
      case DxbcSystemValue::Position:
      case DxbcSystemValue::CullDistance:
      case DxbcSystemValue::ClipDistance:
        emitVsSystemValueStore(sv, mask, value);
        break;
      
      case DxbcSystemValue::RenderTargetId: {
        if (m_gs.builtinLayer == 0) {
          m_gs.builtinLayer = emitNewBuiltinVariable({
            { DxbcScalarType::Uint32, 1, 0 },
            spv::StorageClassOutput },
            spv::BuiltInLayer,
            "gs_layer");
        }
        
        DxbcRegisterPointer ptr;
        ptr.type.ctype   = DxbcScalarType::Uint32;
        ptr.type.ccount  = 1;
        ptr.id = m_gs.builtinLayer;
        
        emitValueStore(
          ptr, emitRegisterExtract(value, mask),
          DxbcRegMask(true, false, false, false));
      } break;
      
      default:
        Logger::warn(str::format(
          "DxbcCompiler: Unhandled GS SV output: ", sv));
    }
  }
  
  
  void DxbcCompiler::emitInit() {
    // Set up common capabilities for all shaders
    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityImageQuery);
    
    // Initialize the shader module with capabilities
    // etc. Each shader type has its own peculiarities.
    switch (m_version.type()) {
      case DxbcProgramType::VertexShader:   emitVsInit(); break;
      case DxbcProgramType::GeometryShader: emitGsInit(); break;
      case DxbcProgramType::PixelShader:    emitPsInit(); break;
      case DxbcProgramType::ComputeShader:  emitCsInit(); break;
      default: throw DxvkError("DxbcCompiler: Unsupported program type");
    }
  }
  
  
  void DxbcCompiler::emitVsInit() {
    m_module.enableCapability(spv::CapabilityClipDistance);
    m_module.enableCapability(spv::CapabilityCullDistance);
    m_module.enableCapability(spv::CapabilityDrawParameters);
    
    m_module.enableExtension("SPV_KHR_shader_draw_parameters");
    
    // Declare the per-vertex output block. This is where
    // the vertex shader will write the vertex position.
    const uint32_t perVertexStruct = this->getPerVertexBlockId();
    const uint32_t perVertexPointer = m_module.defPointerType(
      perVertexStruct, spv::StorageClassOutput);
    
    m_perVertexOut = m_module.newVar(
      perVertexPointer, spv::StorageClassOutput);
    m_entryPointInterfaces.push_back(m_perVertexOut);
    m_module.setDebugName(m_perVertexOut, "vs_vertex_out");
    
    // Standard input array
    emitDclInputArray(0);
    
    // Main function of the vertex shader
    m_vs.functionId = m_module.allocateId();
    m_module.setDebugName(m_vs.functionId, "vs_main");
    
    m_module.functionBegin(
      m_module.defVoidType(),
      m_vs.functionId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
  }
  
  
  void DxbcCompiler::emitGsInit() {
    m_module.enableCapability(spv::CapabilityGeometry);
    m_module.enableCapability(spv::CapabilityClipDistance);
    m_module.enableCapability(spv::CapabilityCullDistance);
    
    // Declare the per-vertex output block. Outputs are not
    // declared as arrays, instead they will be flushed when
    // calling EmitVertex.
    const uint32_t perVertexStruct = this->getPerVertexBlockId();
    const uint32_t perVertexPointer = m_module.defPointerType(
      perVertexStruct, spv::StorageClassOutput);
    
    m_perVertexOut = m_module.newVar(
      perVertexPointer, spv::StorageClassOutput);
    m_entryPointInterfaces.push_back(m_perVertexOut);
    m_module.setDebugName(m_perVertexOut, "gs_vertex_out");
    
    // Main function of the vertex shader
    m_gs.functionId = m_module.allocateId();
    m_module.setDebugName(m_gs.functionId, "gs_main");
    
    m_module.functionBegin(
      m_module.defVoidType(),
      m_gs.functionId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
  }
  
  
  void DxbcCompiler::emitPsInit() {
    m_module.enableCapability(
      spv::CapabilityDerivativeControl);
    
    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);
    
    // Declare pixel shader outputs. According to the Vulkan
    // documentation, they are required to match the type of
    // the render target.
    for (auto e = m_osgn->begin(); e != m_osgn->end(); e++) {
      if (e->systemValue == DxbcSystemValue::None
       && e->registerId  != 0xFFFFFFFF /* depth */) {
        DxbcRegisterInfo info;
        info.type.ctype   = e->componentType;
        info.type.ccount  = e->componentMask.setCount();
        info.type.alength = 0;
        info.sclass = spv::StorageClassOutput;
        
        const uint32_t varId = emitNewVariable(info);
        
        m_module.decorateLocation(varId, e->registerId);
        m_module.setDebugName(varId, str::format("o", e->registerId).c_str());
        m_entryPointInterfaces.push_back(varId);
        
        m_oRegs.at(e->registerId) = varId;
        m_ps.oTypes.at(e->registerId).ctype  = info.type.ctype;
        m_ps.oTypes.at(e->registerId).ccount = info.type.ccount;
      }
    }
    
    // Standard input array
    emitDclInputArray(0);
    
    // Main function of the pixel shader
    m_ps.functionId = m_module.allocateId();
    m_module.setDebugName(m_ps.functionId, "ps_main");
    
    m_module.functionBegin(
      m_module.defVoidType(),
      m_ps.functionId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
  }
  
  
  void DxbcCompiler::emitCsInit() {
    // Main function of the compute shader
    m_cs.functionId = m_module.allocateId();
    m_module.setDebugName(m_cs.functionId, "cs_main");
    
    m_module.functionBegin(
      m_module.defVoidType(),
      m_cs.functionId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
  }
  
  
  void DxbcCompiler::emitVsFinalize() {
    this->emitInputSetup();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_vs.functionId, 0, nullptr);
    this->emitOutputSetup();
  }
  
  
  void DxbcCompiler::emitGsFinalize() {
    this->emitInputSetup(
      primitiveVertexCount(m_gs.inputPrimitive));
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_gs.functionId, 0, nullptr);
    // No output setup at this point as that was
    // already done during the EmitVertex step
  }
  
  
  void DxbcCompiler::emitPsFinalize() {
    this->emitInputSetup();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_ps.functionId, 0, nullptr);
    this->emitOutputSetup();
  }
  
  
  void DxbcCompiler::emitCsFinalize() {
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_cs.functionId, 0, nullptr);
  }
  
  
  void DxbcCompiler::emitDclInputArray(uint32_t vertexCount) {
    DxbcArrayType info;
    info.ctype   = DxbcScalarType::Float32;
    info.ccount  = 4;
    info.alength = DxbcMaxInterfaceRegs;
    
    // Define the array type. This will be two-dimensional
    // in some shaders, with the outer index representing
    // the vertex ID within an invocation.
    uint32_t arrayTypeId = getArrayTypeId(info);
    
    if (vertexCount != 0) {
      arrayTypeId = m_module.defArrayType(
        arrayTypeId, m_module.constu32(vertexCount));
    }
    
    // Define the actual variable. Note that this is private
    // because we will copy input registers and some system
    // variables to the array during the setup phase.
    const uint32_t ptrTypeId = m_module.defPointerType(
      arrayTypeId, spv::StorageClassPrivate);
    
    const uint32_t varId = m_module.newVar(
      ptrTypeId, spv::StorageClassPrivate);
    
    m_module.setDebugName(varId, "shader_in");
    m_vArray = varId;
  }
  
  
  void DxbcCompiler::emitDclInputPerVertex(
          uint32_t          vertexCount,
    const char*             varName) {
    uint32_t typeId = getPerVertexBlockId();
    
    if (vertexCount != 0) {
      typeId = m_module.defArrayType(typeId,
        m_module.constu32(vertexCount));
    }
    
    const uint32_t ptrTypeId = m_module.defPointerType(
      typeId, spv::StorageClassInput);
    
    m_perVertexIn = m_module.newVar(
      ptrTypeId, spv::StorageClassInput);
    m_module.setDebugName(m_perVertexIn, varName);
  }
  
  
  uint32_t DxbcCompiler::emitNewVariable(const DxbcRegisterInfo& info) {
    const uint32_t ptrTypeId = this->getPointerTypeId(info);
    return m_module.newVar(ptrTypeId, info.sclass);
  }
  
  
  uint32_t DxbcCompiler::emitNewBuiltinVariable(
    const DxbcRegisterInfo& info,
          spv::BuiltIn      builtIn,
    const char*             name) {
    const uint32_t varId = emitNewVariable(info);
    
    m_module.decorateBuiltIn(varId, builtIn);
    m_module.setDebugName(varId, name);
    
    m_entryPointInterfaces.push_back(varId);
    return varId;
  }
  
  
  DxbcCfgBlock* DxbcCompiler::cfgFindBlock(
    const std::initializer_list<DxbcCfgBlockType>& types) {
    for (auto cur =  m_controlFlowBlocks.rbegin();
              cur != m_controlFlowBlocks.rend(); cur++) {
      for (auto type : types) {
        if (cur->type == type)
          return &(*cur);
      }
    }
    
    return nullptr;
  }
  
  
  DxbcBufferInfo DxbcCompiler::getBufferInfo(const DxbcRegister& reg) {
    const uint32_t registerId = reg.idx[0].offset;
    
    switch (reg.type) {
      case DxbcOperandType::Resource: {
        DxbcBufferInfo result;
        result.image  = m_textures.at(registerId).imageInfo;
        result.type   = m_textures.at(registerId).type;
        result.typeId = m_textures.at(registerId).imageTypeId;
        result.varId  = m_textures.at(registerId).varId;
        result.specId = m_textures.at(registerId).specId;
        result.stride = m_textures.at(registerId).structStride;
        return result;
      } break;
        
      case DxbcOperandType::UnorderedAccessView: {
        DxbcBufferInfo result;
        result.image  = m_uavs.at(registerId).imageInfo;
        result.type   = m_uavs.at(registerId).type;
        result.typeId = m_uavs.at(registerId).imageTypeId;
        result.varId  = m_uavs.at(registerId).varId;
        result.specId = m_uavs.at(registerId).specId;
        result.stride = m_uavs.at(registerId).structStride;
        return result;
      } break;
        
      case DxbcOperandType::ThreadGroupSharedMemory: {
        DxbcBufferInfo result;
        result.image  = { spv::DimBuffer, 0, 0, 0 };
        result.type   = m_gRegs.at(registerId).type;
        result.typeId = m_module.defPointerType(
          getScalarTypeId(DxbcScalarType::Uint32),
          spv::StorageClassWorkgroup);
        result.varId  = m_gRegs.at(registerId).varId;
        result.specId = 0;
        result.stride = m_gRegs.at(registerId).elementStride;
        return result;
      } break;
        
      default:
        throw DxvkError(str::format("DxbcCompiler: Invalid operand type for buffer: ", reg.type));
    }
  }
  
  
  uint32_t DxbcCompiler::getTexLayerDim(const DxbcImageInfo& imageType) const {
    switch (imageType.dim) {
      case spv::DimBuffer:  return 1;
      case spv::Dim1D:      return 1;
      case spv::Dim2D:      return 2;
      case spv::Dim3D:      return 3;
      case spv::DimCube:    return 3;
      default: throw DxvkError("DxbcCompiler: getTexLayerDim: Unsupported image dimension");
    }
  }
  
  
  uint32_t DxbcCompiler::getTexCoordDim(const DxbcImageInfo& imageType) const {
    return getTexLayerDim(imageType) + imageType.array;
  }
  
  
  DxbcRegMask DxbcCompiler::getTexCoordMask(const DxbcImageInfo& imageType) const {
    return DxbcRegMask::firstN(getTexCoordDim(imageType));
  }
  
  
  DxbcVectorType DxbcCompiler::getInputRegType(uint32_t regIdx) const {
    DxbcVectorType result;
    result.ctype  = DxbcScalarType::Float32;
    result.ccount = 4;
    
    // Vertex shader inputs must match the type of the input layout
    if (m_version.type() == DxbcProgramType::VertexShader) {
      const DxbcSgnEntry* entry = m_isgn->findByRegister(regIdx);
      
      if (entry != nullptr)
        result.ctype = entry->componentType;
    }
    
    return result;
  }
  
  
  VkImageViewType DxbcCompiler::getViewType(DxbcResourceDim dim) const {
    switch (dim) {
      default:
      case DxbcResourceDim::Unknown:
      case DxbcResourceDim::Buffer:
      case DxbcResourceDim::RawBuffer:
      case DxbcResourceDim::StructuredBuffer: return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      case DxbcResourceDim::Texture1D:        return VK_IMAGE_VIEW_TYPE_1D;
      case DxbcResourceDim::Texture1DArr:     return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
      case DxbcResourceDim::Texture2D:        return VK_IMAGE_VIEW_TYPE_2D;
      case DxbcResourceDim::Texture2DMs:      return VK_IMAGE_VIEW_TYPE_2D;
      case DxbcResourceDim::Texture2DArr:     return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      case DxbcResourceDim::Texture2DMsArr:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      case DxbcResourceDim::TextureCube:      return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
      case DxbcResourceDim::TextureCubeArr:   return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
      case DxbcResourceDim::Texture3D:        return VK_IMAGE_VIEW_TYPE_3D;
    }
  }
  
  
  uint32_t DxbcCompiler::getScalarTypeId(DxbcScalarType type) {
    switch (type) {
      case DxbcScalarType::Uint32:  return m_module.defIntType(32, 0);
      case DxbcScalarType::Uint64:  return m_module.defIntType(64, 0);
      case DxbcScalarType::Sint32:  return m_module.defIntType(32, 1);
      case DxbcScalarType::Sint64:  return m_module.defIntType(64, 1);
      case DxbcScalarType::Float32: return m_module.defFloatType(32);
      case DxbcScalarType::Float64: return m_module.defFloatType(64);
      case DxbcScalarType::Bool:    return m_module.defBoolType();
    }
    
    throw DxvkError("DxbcCompiler: Invalid scalar type");
  }
  
  
  uint32_t DxbcCompiler::getVectorTypeId(const DxbcVectorType& type) {
    uint32_t typeId = this->getScalarTypeId(type.ctype);
    
    if (type.ccount > 1)
      typeId = m_module.defVectorType(typeId, type.ccount);
    
    return typeId;
  }
  
  
  uint32_t DxbcCompiler::getArrayTypeId(const DxbcArrayType& type) {
    DxbcVectorType vtype;
    vtype.ctype  = type.ctype;
    vtype.ccount = type.ccount;
    
    uint32_t typeId = this->getVectorTypeId(vtype);
    
    if (type.alength != 0) {
      typeId = m_module.defArrayType(typeId,
        m_module.constu32(type.alength));
    }
    
    return typeId;
  }
  
  
  uint32_t DxbcCompiler::getPointerTypeId(const DxbcRegisterInfo& type) {
    return m_module.defPointerType(
      this->getArrayTypeId(type.type),
      type.sclass);
  }
  
  
  uint32_t DxbcCompiler::getPerVertexBlockId() {
    uint32_t t_f32    = m_module.defFloatType(32);
    uint32_t t_f32_v4 = m_module.defVectorType(t_f32, 4);
//     uint32_t t_f32_a4 = m_module.defArrayType(t_f32, m_module.constu32(4));
    
    std::array<uint32_t, 1> members;
    members[PerVertex_Position] = t_f32_v4;
//     members[PerVertex_CullDist] = t_f32_a4;
//     members[PerVertex_ClipDist] = t_f32_a4;
    
    uint32_t typeId = m_module.defStructTypeUnique(
      members.size(), members.data());
    
    m_module.memberDecorateBuiltIn(typeId, PerVertex_Position, spv::BuiltInPosition);
//     m_module.memberDecorateBuiltIn(typeId, PerVertex_CullDist, spv::BuiltInCullDistance);
//     m_module.memberDecorateBuiltIn(typeId, PerVertex_ClipDist, spv::BuiltInClipDistance);
    m_module.decorateBlock(typeId);
    
    m_module.setDebugName(typeId, "s_per_vertex");
    m_module.setDebugMemberName(typeId, PerVertex_Position, "position");
//     m_module.setDebugMemberName(typeId, PerVertex_CullDist, "cull_dist");
//     m_module.setDebugMemberName(typeId, PerVertex_ClipDist, "clip_dist");
    return typeId;
  }
  
}