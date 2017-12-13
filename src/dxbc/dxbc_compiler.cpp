#include "dxbc_compiler.h"
#include "dxbc_names.h"
#include "dxbc_util.h"

namespace dxvk {
  
  constexpr uint32_t PerVertex_Position  = 0;
  constexpr uint32_t PerVertex_PointSize = 1;
  constexpr uint32_t PerVertex_CullDist  = 2;
  constexpr uint32_t PerVertex_ClipDist  = 3;
  
  
  DxbcCompiler2::DxbcCompiler2(
    const DxbcProgramVersion& version,
    const Rc<DxbcIsgn>&       isgn,
    const Rc<DxbcIsgn>&       osgn)
  : m_version (version),
    m_isgn    (isgn),
    m_osgn    (osgn) {
    // Declare an entry point ID. We'll need it during the
    // initialization phase where the execution mode is set.
    m_entryPointId = m_module.allocateId();
    
    // Set the memory model. This is the same for all shaders.
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
    
    // Make sure our interface registers don't
    // contain any valid IDs at the moment.
    for (size_t i = 0; i < DxbcMaxInterfaceRegs; i++) {
      m_vRegs[i] = 0;
      m_oRegs[i] = 0;
    }
    
    // Initialize the shader module with capabilities
    // etc. Each shader type has its own peculiarities.
    switch (m_version.type()) {
      case DxbcProgramType::PixelShader:  this->beginPixelShader (osgn); break;
      case DxbcProgramType::VertexShader: this->beginVertexShader(isgn); break;
      default: Logger::err("dxbc: Unsupported shader type");
    }
  }
  
  
  DxbcCompiler2::~DxbcCompiler2() {
    
  }
  
  
  DxbcError DxbcCompiler2::processInstruction(const DxbcInstruction& ins) {
    DxbcInst parsedInst;
    DxbcError parseError = this->parseInstruction(ins, parsedInst);
    
    if (parseError != DxbcError::sOk)
      return parseError;
    
    switch (parsedInst.format.instructionClass) {
      case DxbcInstClass::Declaration:    return this->handleDeclaration  (parsedInst);
      case DxbcInstClass::ControlFlow:    return this->handleControlFlow  (parsedInst);
      case DxbcInstClass::TextureSample:  return this->handleTextureSample(parsedInst);
      case DxbcInstClass::VectorAlu:      return this->handleVectorAlu    (parsedInst);
      case DxbcInstClass::VectorDot:      return this->handleVectorDot    (parsedInst);
      default:                            return DxbcError::eUnhandledOpcode;
    }
  }
  
  
  Rc<DxvkShader> DxbcCompiler2::finalize() {
    // Define the actual 'main' function of the shader
    m_module.functionBegin(
      m_module.defVoidType(),
      m_entryPointId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
    
    // Depending on the shader type, this will prepare input registers,
    // call various shader functions and write back the output registers.
    switch (m_version.type()) {
      case DxbcProgramType::PixelShader:  this->endPixelShader (); break;
      case DxbcProgramType::VertexShader: this->endVertexShader(); break;
      default: Logger::err("dxbc: Unsupported shader type");
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
      m_module.compile());
  }
  
  
  DxbcError DxbcCompiler2::handleDeclaration(const DxbcInst& ins) {
    switch (ins.opcode) {
      case DxbcOpcode::DclGlobalFlags:
        return this->declareGlobalFlags(ins);
        
      case DxbcOpcode::DclTemps:
        return this->declareTemps(ins);
      
      case DxbcOpcode::DclInput:
      case DxbcOpcode::DclInputSiv:
      case DxbcOpcode::DclInputSgv:
      case DxbcOpcode::DclInputPs:
      case DxbcOpcode::DclInputPsSiv:
      case DxbcOpcode::DclInputPsSgv:
      case DxbcOpcode::DclOutput:
      case DxbcOpcode::DclOutputSiv:
      case DxbcOpcode::DclOutputSgv:
        return this->declareInterfaceVar(ins);
      
      case DxbcOpcode::DclConstantBuffer:
        return this->declareConstantBuffer(ins);
      
      case DxbcOpcode::DclSampler:
        return this->declareSampler(ins);
      
      case DxbcOpcode::DclResource:
        return this->declareResource(ins);
      
      default:
        return DxbcError::eUnhandledOpcode;
    }
  }
  
  
  DxbcError DxbcCompiler2::declareGlobalFlags(const DxbcInst& ins) {
    // TODO add support for double-precision floats
    // TODO add support for early depth-stencil
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::declareTemps(const DxbcInst& ins) {
    if (ins.operands[0].type != DxbcOperandType::Imm32) {
      Logger::err("dxbc: Number of temps not a contant");
      return DxbcError::eInvalidOperand;
    }
    
    // Some shader program types use multiple sets of temps,
    // so we'll just check if we need to create new ones.
    const uint32_t newSize = ins.operands[0].immediates[0];
    const uint32_t oldSize = m_rRegs.size();
    
    if (newSize > oldSize) {
      m_rRegs.resize(newSize);
      
      // r# registers are always 4x32-bit float vectors
      const uint32_t regTypeId = this->defineVectorType(
        DxbcScalarType::Float32, 4);
      
      const uint32_t ptrTypeId = m_module.defPointerType(
        regTypeId, spv::StorageClassPrivate);
      
      for (uint32_t i = oldSize; i < newSize; i++) {
        m_rRegs.at(i) = m_module.newVar(
          ptrTypeId, spv::StorageClassPrivate);
        
        m_module.setDebugName(m_rRegs.at(i),
          str::format("r", i).c_str());
      }
    }
    
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::declareInterfaceVar(const DxbcInst& ins) {
    const DxbcInstOp& op = ins.operands[0];
    
    // In the vertex and fragment shader stage, the
    // operand indices will have the following format:
    //    (0) Register index
    // 
    // In other stages, the input and output registers
    // are declared as arrays of a fixed size:
    //    (0) Array size
    //    (1) Register index
    uint32_t regId  = 0;
    uint32_t regDim = 0;
    
    if (op.indexDim == 1) {
      if (op.index[0].type != DxbcIndexType::Immediate)
        return DxbcError::eInvalidOperandIndex;
      
      regId = op.index[0].immediate;
    } else if (op.indexDim == 2) {
      if (op.index[0].type != DxbcIndexType::Immediate
        || op.index[1].type != DxbcIndexType::Immediate)
        return DxbcError::eInvalidOperandIndex;
      
      regDim = op.index[0].immediate;
      regId  = op.index[1].immediate;
    } else {
      Logger::err("dxbc: Invalid index dimension for v#/o# declaration");
      return DxbcError::eInvalidOperandIndex;
    }
    
    // This declaration may map an output register to a system
    // value. If that is the case, the system value type will
    // be stored in the second operand.
    const bool hasSv =
        ins.opcode == DxbcOpcode::DclInputSgv
      || ins.opcode == DxbcOpcode::DclInputSiv
      || ins.opcode == DxbcOpcode::DclInputPsSgv
      || ins.opcode == DxbcOpcode::DclInputPsSiv
      || ins.opcode == DxbcOpcode::DclOutputSgv
      || ins.opcode == DxbcOpcode::DclOutputSiv;
    
    DxbcSystemValue sv = DxbcSystemValue::None;
    
    if (hasSv) {
      if (ins.operands[1].type != DxbcOperandType::Imm32) {
        Logger::err("dxbc: Invalid system value in v#/o# declaration");
        return DxbcError::eInstructionFormat;
      }
      
      sv = static_cast<DxbcSystemValue>(
        ins.operands[1].immediates[0]);
    }
    
    // In the pixel shader, inputs are declared with an
    // interpolation mode that is part of the op token.
//         const bool hasInterpolationMode =
//             ins.opcode == DxbcOpcode::DclInputPs
//          || ins.opcode == DxbcOpcode::DclInputPsSiv;
    
    DxbcInterpolationMode im = DxbcInterpolationMode::Undefined;
    
    // TODO implement this
//         if (hasInterpolationMode) {
//           im = static_cast<DxbcInterpolationMode>(
//             bit::extract(ins.token().control(), 0, 3));
//         }
    
    // Declare the actual variable
    switch (op.type) {
      case DxbcOperandType::Input:
        return this->declareInputVar(
          regId, regDim, op.mask, sv, im);
        
      case DxbcOperandType::Output:
        return this->declareOutputVar(
          regId, regDim, op.mask, sv, im);
        
      default:
        // We shouldn't ever be here
        return DxbcError::eInternal;
    }
  }
  
  
  DxbcError DxbcCompiler2::declareConstantBuffer(const DxbcInst& ins) {
    const DxbcInstOp& op = ins.operands[0];
    
    // This instruction has one operand with two indices:
    //  (1) Constant buffer register ID (cb#)
    //  (2) Number of constants in the buffer
    if (op.indexDim != 2) {
      Logger::err("dxbc: Constant buffer declaration requires two indices");
      return DxbcError::eInvalidOperandIndex;
    }
    
    const uint32_t bufferId     = op.index[0].immediate;
    const uint32_t elementCount = op.index[1].immediate;
    
    // Uniform buffer data is stored as a fixed-size array
    // of 4x32-bit vectors. SPIR-V requires explicit strides.
    uint32_t arrayType = m_module.defArrayTypeUnique(
      this->defineVectorType(DxbcScalarType::Float32, 4),
      m_module.constu32(elementCount));
    m_module.decorateArrayStride(arrayType, 16);
    
    // SPIR-V requires us to put that array into a
    // struct and decorate that struct as a block.
    uint32_t structType = m_module.defStructTypeUnique(1, &arrayType);
    m_module.memberDecorateOffset(structType, 0, 0);
    m_module.decorateBlock(structType);
    
    // Variable that we'll use to access the buffer
    uint32_t varId = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);
    
    m_module.setDebugName(varId,
      str::format("cb", bufferId).c_str());
    
    m_constantBuffers.at(bufferId).varId = varId;
    m_constantBuffers.at(bufferId).size  = elementCount;
    
    // Compute the DXVK binding slot index for the buffer.
    // D3D11 needs to bind the actual buffers to this slot.
    uint32_t bindingId = computeResourceSlotId(
      m_version.type(), DxbcBindingType::ConstantBuffer,
      bufferId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    m_resourceSlots.push_back(resource);
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::declareSampler(const DxbcInst& ins) {
    // dclSampler takes one operand:
    //  (1) The sampler register ID
    // TODO implement sampler mode (default / comparison / mono)
    if (ins.operands[0].indexDim != 1) {
      Logger::err("dxbc: Invalid sampler index dimension");
      return DxbcError::eInvalidOperandIndex;
    }
    
    uint32_t samplerId = ins.operands[0].index[0].immediate;
    
    // The sampler type is opaque, but we still have to
    // define a pointer and a variable in oder to use it
    uint32_t samplerType = m_module.defSamplerType();
    uint32_t samplerPtrType = m_module.defPointerType(
      samplerType, spv::StorageClassUniformConstant);
    
    // Define the sampler variable
    uint32_t varId = m_module.newVar(samplerPtrType,
      spv::StorageClassUniformConstant);
    
    m_module.setDebugName(varId,
      str::format("s", samplerId).c_str());
    
    m_samplers.at(samplerId).varId  = varId;
    m_samplers.at(samplerId).typeId = samplerType;
    
    // Compute binding slot index for the sampler
    uint32_t bindingId = computeResourceSlotId(
      m_version.type(), DxbcBindingType::ImageSampler, samplerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    m_resourceSlots.push_back(resource);
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::declareResource(const DxbcInst& ins) {
    // dclResource takes two operands:
    //  (1) The resource register ID
    //  (2) The resource return type
    const DxbcInstOp op = ins.operands[0];
    
    if (op.indexDim != 1) {
      Logger::err("dxbc: dclResource: Invalid index dimension");
      return DxbcError::eInvalidOperandIndex;
    }
    
    const uint32_t registerId = op.index[0].immediate;
    
    // Defines the type of the resource (texture2D, ...)
    const DxbcResourceDim resourceType = ins.control.resourceDim();
    
    // Defines the type of a read operation. DXBC has the ability
    // to define four different types whereas SPIR-V only allows
    // one, but in practice this should not be much of a problem.
    auto xType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.operands[1].immediates[0], 0, 3));
    auto yType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.operands[1].immediates[0], 4, 7));
    auto zType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.operands[1].immediates[0], 8, 11));
    auto wType = static_cast<DxbcResourceReturnType>(
      bit::extract(ins.operands[1].immediates[0], 12, 15));
    
    if ((xType != yType) || (xType != zType) || (xType != wType))
      Logger::warn("DXBC: dclResource: Ignoring resource return types");
    
    // Declare the actual sampled type
    uint32_t sampledTypeId = 0;
    
    switch (xType) {
      case DxbcResourceReturnType::Float: sampledTypeId = m_module.defFloatType(32);    break;
      case DxbcResourceReturnType::Sint:  sampledTypeId = m_module.defIntType  (32, 1); break;
      case DxbcResourceReturnType::Uint:  sampledTypeId = m_module.defIntType  (32, 0); break;
      default:
        Logger::err(str::format("dxbc: Invalid sampled type: ", xType));
        return DxbcError::eInvalidOperand;
    }
    
    // Declare the resource type
    uint32_t textureTypeId = 0;
    
    switch (resourceType) {
      case DxbcResourceDim::Texture1D:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim1D, 0, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture1DArr:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim1D, 0, 1, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture2D:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim2D, 0, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture2DArr:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim2D, 0, 1, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture3D:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim3D, 0, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::TextureCube:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::DimCube, 0, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::TextureCubeArr:
        textureTypeId = m_module.defImageType(
          sampledTypeId, spv::DimCube, 0, 1, 0, 1,
          spv::ImageFormatUnknown);
        break;
        
      default:
        Logger::err(str::format("dxbc: Unsupported resource type: ", resourceType));
        return DxbcError::eUnsupported;
    }
    
    uint32_t resourcePtrType = m_module.defPointerType(
      textureTypeId, spv::StorageClassUniformConstant);
    
    uint32_t varId = m_module.newVar(resourcePtrType,
      spv::StorageClassUniformConstant);
    
    m_module.setDebugName(varId,
      str::format("t", registerId).c_str());
    
    m_textures.at(registerId).varId         = varId;
    m_textures.at(registerId).sampledTypeId = sampledTypeId;
    m_textures.at(registerId).textureTypeId = textureTypeId;
    
    // Compute the DXVK binding slot index for the resource.
    // D3D11 needs to bind the actual resource to this slot.
    uint32_t bindingId = computeResourceSlotId(m_version.type(),
      DxbcBindingType::ShaderResource, registerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    m_resourceSlots.push_back(resource);
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::declareInputVar(
          uint32_t              regId,
          uint32_t              regDim,
          DxbcRegMask           regMask,
          DxbcSystemValue       sv,
          DxbcInterpolationMode im) {
    if (regDim != 0) {
      Logger::err("dxbc: Input arrays not yet supported");
      return DxbcError::eUnsupported;
    }
    
    // A variable may be declared multiple
    // times when system values are involved
    if (m_vRegs.at(regId) == 0) {
      uint32_t regTypeId = m_module.defVectorType(
        m_module.defFloatType(32), 4);
      
      uint32_t ptrTypeId = m_module.defPointerType(
        regTypeId, spv::StorageClassInput);
      
      uint32_t varId = m_module.newVar(
        ptrTypeId, spv::StorageClassInput);
      
      m_module.decorateLocation(varId, regId);
      m_module.setDebugName(varId, str::format("v", regId).c_str());
      m_entryPointInterfaces.push_back(varId);
      
      m_vRegs.at(regId) = varId;
    }
    
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::declareOutputVar(
          uint32_t              regId,
          uint32_t              regDim,
          DxbcRegMask           regMask,
          DxbcSystemValue       sv,
          DxbcInterpolationMode im) {
    if (regDim != 0) {
      Logger::err("dxbc: Output arrays not yet supported");
      return DxbcError::eUnsupported;
    }
    
    // Fragment shader outputs were defined earlier, based
    // on their signature. We cannot add new output registers.
    if (m_version.type() == DxbcProgramType::PixelShader)
      return DxbcError::sOk;
    
    // Output variables may also be defined multiple times since
    // multiple vector components may be mapped to system values.
    if (m_oRegs.at(regId) == 0) {
      uint32_t regTypeId = m_module.defVectorType(
        m_module.defFloatType(32), 4);
      
      uint32_t ptrTypeId = m_module.defPointerType(
        regTypeId, spv::StorageClassOutput);
      
      uint32_t varId = m_module.newVar(
        ptrTypeId, spv::StorageClassOutput);
      
      m_module.decorateLocation(varId, regId);
      m_module.setDebugName(varId, str::format("o", regId).c_str());
      m_entryPointInterfaces.push_back(varId);
      
      m_oRegs.at(regId) = varId;
    }
    
    // Add a new system value mapping if needed
    // TODO declare SV if necessary
    if (sv != DxbcSystemValue::None)
      m_oSvs.push_back({ regId, regMask, sv });
    
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::handleControlFlow(const DxbcInst& ins) {
    switch (ins.opcode) {
      case DxbcOpcode::Ret:
        m_module.opReturn();
        m_module.functionEnd();
        return DxbcError::sOk;
      
      default:
        return DxbcError::eUnhandledOpcode;
    }
  }
  
  
  DxbcError DxbcCompiler2::handleTextureSample(const DxbcInst& ins) {
    // TODO support address offset
    // TODO support more sample ops
    
    // sample has four operands:
    //  (1) The destination register
    //  (2) Texture coordinates
    //  (3) The texture itself
    //  (4) The sampler object
    const DxbcInstOp destOp    = ins.operands[0];
    const DxbcInstOp coordOp   = ins.operands[1];
    const DxbcInstOp textureOp = ins.operands[2];
    const DxbcInstOp samplerOp = ins.operands[3];
    
    if (textureOp.indexDim != 1 || samplerOp.indexDim != 1) {
      Logger::err("dxbc: Texture and Sampler registers require one index");
      return DxbcError::eInvalidOperandIndex;
    }
    
    // Texture and sampler register IDs
    const uint32_t textureId = textureOp.index[0].immediate;
    const uint32_t samplerId = samplerOp.index[0].immediate;
    
    // Load the texture coordinates. SPIR-V allows these
    // to be float4 even if not all components are used.
    const DxbcValue2 coord = this->loadOp(coordOp,
      DxbcRegMask(true, true, true, true),
      DxbcScalarType::Float32);
    
    // Combine the texture and the sampler into a sampled image
    uint32_t sampledImageType = m_module.defSampledImageType(
      m_textures.at(textureId).textureTypeId);
    
    uint32_t sampledImageId = m_module.opSampledImage(
      sampledImageType,
      m_module.opLoad(
        m_textures.at(textureId).textureTypeId,
        m_textures.at(textureId).varId),
      m_module.opLoad(
        m_samplers.at(samplerId).typeId,
        m_samplers.at(samplerId).varId));
    
    // Sampling an image in SPIR-V always returns a four-component
    // vector, so we need to declare the corresponding type here
    // TODO infer sampled type properly
    DxbcValue2 result;
    result.componentType  = DxbcScalarType::Float32;
    result.componentCount = 4;
    result.valueId = m_module.opImageSampleImplicitLod(
      this->defineVectorType(result.componentType, result.componentCount),
      sampledImageId, coord.valueId);
    
    // Swizzle components using the texture swizzle
    // and the destination operand's write mask
    result = this->swizzleReg(result, textureOp.swizzle, destOp.mask);
    
    this->storeOp(destOp, result);
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::handleVectorAlu(const DxbcInst& ins) {
    // Load input operands. Operands that are floating
    // point types will be affected by modifiers.
    DxbcValue2 arguments[DxbcMaxOperandCount - 1];
    
    for (uint32_t i = 1; i < ins.format.operandCount; i++) {
      arguments[i - 1] = this->loadOp(
        ins.operands[i],
        ins.operands[0].mask,
        ins.format.operands[i].type);
    }
    
    // Result that we will write to the destination operand
    DxbcValue2 result;
    result.componentType  = arguments[0].componentType;
    result.componentCount = arguments[0].componentCount;
    
    uint32_t resultTypeId = this->defineVectorType(
      result.componentType, result.componentCount);
    
    switch (ins.opcode) {
      case DxbcOpcode::Add:
        result.valueId = m_module.opFAdd(
          resultTypeId,
          arguments[0].valueId,
          arguments[1].valueId);
        break;
      
      case DxbcOpcode::Mad:
        result.valueId = m_module.opFFma(
          resultTypeId,
          arguments[0].valueId,
          arguments[1].valueId,
          arguments[2].valueId);
        break;
      
      case DxbcOpcode::Mov:
        result.valueId = arguments[0].valueId;
        break;
      
      case DxbcOpcode::Mul:
        result.valueId = m_module.opFMul(
          resultTypeId,
          arguments[0].valueId,
          arguments[1].valueId);
        break;
      
      case DxbcOpcode::Rsq:
        result.valueId = m_module.opInverseSqrt(
          resultTypeId, arguments[0].valueId);
        break;
      
      default:
        return DxbcError::eUnhandledOpcode;
    }
    
    // Apply result modifiers to floating-point results
    result = this->applyResultModifiers(result, ins.control);
    this->storeOp(ins.operands[0], result);
    return DxbcError::sOk;
  }
  
  
  DxbcError DxbcCompiler2::handleVectorDot(const DxbcInst& ins) {
    // Determine the component count and the source
    // operand mask. Since the result is scalar, we
    // cannot use the destination register mask.
    uint32_t numComponents = 0;
    
    switch (ins.opcode) {
      case DxbcOpcode::Dp2: numComponents = 2; break;
      case DxbcOpcode::Dp3: numComponents = 3; break;
      case DxbcOpcode::Dp4: numComponents = 4; break;
      default: return DxbcError::eUnhandledOpcode;
    }
    
    // We'll use xyz for dp3, xy for dp2
    const DxbcRegMask srcMask(
      numComponents >= 1, numComponents >= 2,
      numComponents >= 3, numComponents >= 4);
    
    // Load input operands as floatig point numbers
    DxbcValue2 arguments[2];
    
    for (uint32_t i = 1; i <= 2; i++) {
      arguments[i - 1] = this->loadOp(
        ins.operands[i], srcMask,
        DxbcScalarType::Float32);
    }
    
    DxbcValue2 result;
    result.componentType  = DxbcScalarType::Float32;
    result.componentCount = 1;
    result.valueId        = m_module.opDot(
      this->defineVectorType(
        result.componentType,
        result.componentCount),
      arguments[0].valueId,
      arguments[1].valueId);
    
    // Apply result modifiers to floating-point results
    result = this->applyResultModifiers(result, ins.control);
    this->storeOp(ins.operands[0], result);
    return DxbcError::sOk;
  }
  
  
  DxbcValue2 DxbcCompiler2::bitcastReg(
    const DxbcValue2&           src,
          DxbcScalarType        type) {
    if (src.componentType == type)
      return src;
    
    // TODO support 64-bit types by adjusting the component count
    uint32_t typeId = this->defineVectorType(type, src.componentCount);
    
    DxbcValue2 result;
    result.componentType  = type;
    result.componentCount = src.componentCount;
    result.valueId = m_module.opBitcast(typeId, src.valueId);
    return result;
  }
  
  
  DxbcValue2 DxbcCompiler2::insertReg(
    const DxbcValue2&           dst,
    const DxbcValue2&           src,
          DxbcRegMask           mask) {
    DxbcValue2 result;
    result.componentType  = dst.componentType;
    result.componentCount = dst.componentCount;
    
    const uint32_t resultTypeId = this->defineVectorType(
      result.componentType, result.componentCount);
    
    if (dst.componentCount == 1) {
      // Both values are scalar, so the first component
      // of the write mask decides which one to take.
      result.valueId = mask[0] ? src.valueId : dst.valueId;
    } else if (src.componentCount == 1) {
      // The source value is scalar. Since OpVectorShuffle
      // requires both arguments to be vectors, we have to
      // use OpCompositeInsert to modify the vector instead.
      const uint32_t componentId = mask.firstSet();
      
      result.valueId = m_module.opCompositeInsert(
        resultTypeId, src.valueId, dst.valueId,
        1, &componentId);
    } else {
      // Both arguments are vectors. We can determine which
      // components to take from which vector and use the
      // OpVectorShuffle instruction.
      uint32_t components[4];
      uint32_t srcComponentId = dst.componentCount;
      
      for (uint32_t i = 0; i < dst.componentCount; i++)
        components[i] = mask[i] ? srcComponentId++ : i;
      
      result.valueId = m_module.opVectorShuffle(
        resultTypeId, dst.valueId, src.valueId,
        dst.componentCount, components);
    }
    
    return result;
  }
  
  
  DxbcValue2 DxbcCompiler2::extractReg(
    const DxbcValue2&           src,
          DxbcRegMask           mask) {
    return this->swizzleReg(src,
      DxbcRegSwizzle(0, 1, 2, 3), mask);
  }
  
  
  DxbcValue2 DxbcCompiler2::swizzleReg(
    const DxbcValue2&           src,
    const DxbcRegSwizzle&       swizzle,
          DxbcRegMask           mask) {
    std::array<uint32_t, 4> indices;
    
    uint32_t dstIndex = 0;
    for (uint32_t i = 0; i < src.componentCount; i++) {
      if (mask[i])
        indices[dstIndex++] = swizzle[i];
    }
    
    // If the swizzle combined with the mask can be reduced
    // to a no-op, we don't need to insert any instructions.
    bool isIdentitySwizzle = dstIndex == src.componentCount;
    
    for (uint32_t i = 0; i < dstIndex && isIdentitySwizzle; i++)
      isIdentitySwizzle &= indices[i] == i;
    
    if (isIdentitySwizzle)
      return src;
    
    // Use OpCompositeExtract if the resulting vector contains
    // only one component, and OpVectorShuffle if it is a vector.
    DxbcValue2 result;
    result.componentType  = src.componentType;
    result.componentCount = dstIndex;
    
    const uint32_t resultTypeId = this->defineVectorType(
      result.componentType, result.componentCount);
      
    if (dstIndex == 1) {
      result.valueId = m_module.opCompositeExtract(
        resultTypeId, src.valueId, 1, indices.data());
    } else {
      result.valueId = m_module.opVectorShuffle(
        resultTypeId, src.valueId, src.valueId,
        dstIndex, indices.data());
    }
    
    return result;
  }
  
  
  DxbcValue2 DxbcCompiler2::extendReg(
    const DxbcValue2&           src,
          uint32_t              size) {
    if (size == 1)
      return src;
    
    std::array<uint32_t, 4> ids = {
      src.valueId, src.valueId,
      src.valueId, src.valueId, 
    };
    
    uint32_t typeId = this->defineVectorType(
      src.componentType, size);
    
    DxbcValue2 result;
    result.componentType  = src.componentType;
    result.componentCount = size;
    result.valueId = m_module.opCompositeConstruct(
      typeId, size, ids.data());
    return result;
  }
  
  
  DxbcValue2 DxbcCompiler2::applyOperandModifiers(
        DxbcValue2            value,
        DxbcOperandModifiers  modifiers) {
    uint32_t typeId = this->defineVectorType(
      value.componentType, value.componentCount);
    
    // Both modifiers can be applied to the same value.
    // In that case, the absolute value is negated.
    if (modifiers.test(DxbcOperandModifier::Abs))
      value.valueId = m_module.opFAbs(typeId, value.valueId);
    
    if (modifiers.test(DxbcOperandModifier::Neg))
      value.valueId = m_module.opFNegate(typeId, value.valueId);
    
    return value;
  }
  
  
  DxbcValue2 DxbcCompiler2::applyResultModifiers(
        DxbcValue2            value,
        DxbcOpcodeControl     control) {
    uint32_t typeId = this->defineVectorType(
      value.componentType, value.componentCount);
    
    if (control.saturateBit()) {
      value.valueId = m_module.opFClamp(
        typeId, value.valueId,
        m_module.constf32(0.0f),
        m_module.constf32(1.0f));
    }
    
    return value;
  }
  
  
  DxbcValue2 DxbcCompiler2::loadOp(
    const DxbcInstOp&           srcOp,
          DxbcRegMask           srcMask,
          DxbcScalarType        dstType) {
    if (srcOp.type == DxbcOperandType::Imm32) {
      return this->loadImm32(srcOp, srcMask, dstType);
    } else {
      // Load operand value from the operand pointer
      DxbcValue2 result = this->loadRegister(srcOp, srcMask, dstType);
      
      // Apply the component swizzle or the selection,
      // depending on which mode the operand is in.
      if (srcOp.componentCount == 4) {
        switch (srcOp.componentMode) {
          case DxbcRegMode::Swizzle:
            result = this->swizzleReg(result, srcOp.swizzle, srcMask);
            break;
          
          case DxbcRegMode::Select1:
            result = this->extractReg(result, 1u << srcOp.select1);
            break;
          
          default:
            Logger::err("dxbc: Invalid component selection mode");
        }
      }
      
      // Cast it to the requested type. We need to do
      // this after the swizzling for 64-bit types.
      if (result.componentType != dstType)
        result = this->bitcastReg(result, dstType);
      
      // Apply operand modifiers
      return this->applyOperandModifiers(result, srcOp.modifiers);
    }
  }
  
  
  DxbcValue2 DxbcCompiler2::loadImm32(
    const DxbcInstOp&           srcOp,
          DxbcRegMask           srcMask,
          DxbcScalarType        dstType) {
    // We will generate Uint32 constants because at this
    // point we don't know how they are going to be used.
    DxbcValue2 result;
    result.componentType  = DxbcScalarType::Uint32;
    result.componentCount = srcMask.setCount();
    
    uint32_t resultTypeId = this->defineVectorType(
      result.componentType, result.componentCount);
    
    uint32_t constIds[4];
    uint32_t constIdx = 0;
    
    // Generate scalar constants for each component
    // and pack them tightly in an array, so that we
    // can use them to generate a constant vector.
    if (srcOp.componentCount == 4) {
      for (uint32_t i = 0; i < 4; i++) {
        if (srcMask[i]) {
          constIds[constIdx++] = m_module.constu32(
            srcOp.immediates[i]);
        }
      }
    } else if (srcOp.componentCount == 1) {
      constIds[0] = m_module.constu32(srcOp.immediates[0]);
    } else {
      Logger::err("dxbc: Invalid imm32 component count");
    }
    
    // If the result is a vector, emit the final constant
    if (result.componentCount == 1) {
      result.valueId = constIds[0];
    } else {
      result.valueId = m_module.constComposite(
        resultTypeId, result.componentCount, constIds);
    }
    
    // If necessary, cast the constant to the desired type
    if (result.componentType != dstType)
      result = this->bitcastReg(result, dstType);
    
    return result;
  }
  
  
  DxbcValue2 DxbcCompiler2::loadRegister(
    const DxbcInstOp&           srcOp,
          DxbcRegMask           srcMask,
          DxbcScalarType        dstType) {
    return this->loadPtr(
      this->getOperandPtr(srcOp));
  }
  
  
  void DxbcCompiler2::storeOp(
    const DxbcInstOp&           dstOp,
    const DxbcValue2&           srcValue) {
    this->storePtr(
      this->getOperandPtr(dstOp),
      srcValue, dstOp.mask);
  }
  
  
  DxbcValue2 DxbcCompiler2::loadPtr(const DxbcPointer2& ptr) {
    const uint32_t typeId = this->defineVectorType(
      ptr.componentType, ptr.componentCount);
    
    DxbcValue2 result;
    result.componentType  = ptr.componentType;
    result.componentCount = ptr.componentCount;
    result.valueId = m_module.opLoad(typeId, ptr.pointerId);
    return result;
  }
  
  
  void DxbcCompiler2::storePtr(
    const DxbcPointer2&         ptr,
    const DxbcValue2&           value,
          DxbcRegMask           mask) {
    DxbcValue2 srcValue = value;
    
    // If the source value consists of only one component,
    // it is stored in all destination register components.
    if (srcValue.componentCount == 1)
      srcValue = this->extendReg(srcValue, mask.setCount());
    
    // If the component types are not compatible,
    // we need to bit-cast the source variable.
    if (ptr.componentType != srcValue.componentType)
      srcValue = this->bitcastReg(srcValue, ptr.componentType);
    
    if (mask.setCount() == ptr.componentCount) {
      // Simple case: We write to the entire register
      m_module.opStore(ptr.pointerId, srcValue.valueId);
    } else {
      // We only write to part of the destination
      // register, so we need to load and modify it
      DxbcValue2 tmp = this->loadPtr(ptr);
                 tmp = this->insertReg(tmp, srcValue, mask);
      
      m_module.opStore(ptr.pointerId, tmp.valueId);
    }
    
  }
  
  
  DxbcValue2 DxbcCompiler2::loadIndex(const DxbcInstOpIndex& idx) {
    DxbcValue2 constantPart;
    DxbcValue2 relativePart;
    
    if ((idx.type == DxbcIndexType::Immediate) || (idx.immediate != 0)) {
      constantPart.componentType  = DxbcScalarType::Sint32;
      constantPart.componentCount = 1;
      constantPart.valueId = m_module.consti32(
        static_cast<int32_t>(idx.immediate));
    }
    
    if (idx.type == DxbcIndexType::Relative) {
      DxbcInstOp offsetOp;
      offsetOp.type               = DxbcOperandType::Temp;
      offsetOp.indexDim           = 1;
      offsetOp.index[0].type      = DxbcIndexType::Immediate;
      offsetOp.index[0].immediate = idx.tempRegId;
      offsetOp.componentCount     = 4;
      offsetOp.componentMode      = DxbcRegMode::Select1;
      offsetOp.select1            = idx.tempRegComponent;
      
      relativePart = this->loadOp(offsetOp,
        DxbcRegMask(true, false, false, false),
        DxbcScalarType::Sint32);
    }
    
    if (relativePart.valueId == 0) return constantPart;
    if (constantPart.valueId == 0) return relativePart;
    
    DxbcValue2 result;
    result.componentType  = DxbcScalarType::Sint32;
    result.componentCount = 1;
    result.valueId        = m_module.opIAdd(
      this->defineScalarType(result.componentType),
      relativePart.valueId, constantPart.valueId);
    return result;
  }
  
  
  DxbcPointer2 DxbcCompiler2::getOperandPtr(const DxbcInstOp& op) {
    DxbcPointer2 result;
    
    switch (op.type) {
      case DxbcOperandType::Temp:
        result.componentType  = DxbcScalarType::Float32;
        result.componentCount = 4;
        result.pointerId      = m_rRegs.at(op.index[0].immediate);
        break;
        
      // TODO implement properly
      case DxbcOperandType::Input:
        result.componentType  = DxbcScalarType::Float32;
        result.componentCount = 4;
        result.pointerId      = m_vRegs.at(op.index[0].immediate);
        break;
      
      // TODO implement properly
      case DxbcOperandType::Output:
        if (m_version.type() == DxbcProgramType::PixelShader)
          return m_ps.oregs.at(op.index[0].immediate);
        
        result.componentType  = DxbcScalarType::Float32;
        result.componentCount = 4;
        result.pointerId      = m_oRegs.at(op.index[0].immediate);
        break;
      
      case DxbcOperandType::ConstantBuffer:
        return this->getConstantBufferPtr(op);
      
      default:
        Logger::err("dxbc: Unhandled operand type");
    }
    
    return result;
  }
  
  
  DxbcPointer2 DxbcCompiler2::getConstantBufferPtr(const DxbcInstOp& op) {
    if (op.indexDim != 2) {
      Logger::err("dxbc: Constant buffer reference needs two indices");
      return DxbcPointer2();
    }
    
    // The operand itself has two indices:
    //  (1) The constant buffer ID (immediate)
    //  (2) The constant offset (relative)
    const uint32_t bufferId = op.index[0].immediate;
    const DxbcValue2 offset = this->loadIndex(op.index[1]);
    
    // The first index selects the struct member,
    // the second one selects the array element.
    std::array<uint32_t, 2> indices = { 
      m_module.constu32(0), offset.valueId };
    
    DxbcPointer2 result;
    result.componentType  = DxbcScalarType::Float32;
    result.componentCount = 4;
    result.pointerId = m_module.opAccessChain(
      this->definePointerType(
        result.componentType,
        result.componentCount,
        spv::StorageClassUniform),
      m_constantBuffers.at(bufferId).varId,
      2, indices.data());
    return result;
  }
  
  
  void DxbcCompiler2::beginVertexShader(const Rc<DxbcIsgn>& isgn) {
    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityCullDistance);
    m_module.enableCapability(spv::CapabilityClipDistance);
    
    // Declare the per-vertex output block. This is where
    // the vertex shader will write the vertex position.
    uint32_t perVertexStruct = this->definePerVertexBlock();
    uint32_t perVertexPointer = m_module.defPointerType(
      perVertexStruct, spv::StorageClassOutput);
    
    m_perVertexOut = m_module.newVar(
      perVertexPointer, spv::StorageClassOutput);
    m_entryPointInterfaces.push_back(m_perVertexOut);
    m_module.setDebugName(m_perVertexOut, "vs_block");
    
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
  
  
  void DxbcCompiler2::beginPixelShader(const Rc<DxbcIsgn>& osgn) {
    m_module.enableCapability(spv::CapabilityShader);
    m_module.setOriginUpperLeft(m_entryPointId);
    
    // Declare pixel shader outputs. According to the Vulkan
    // documentation, they are required to match the type of
    // the render target.
    for (auto e = m_osgn->begin(); e != m_osgn->end(); e++) {
      if (e->systemValue == DxbcSystemValue::None) {
        uint32_t regTypeId = this->defineVectorType(
          e->componentType, e->componentMask.componentCount());
        
        uint32_t ptrTypeId = m_module.defPointerType(
          regTypeId, spv::StorageClassOutput);
        
        uint32_t varId = m_module.newVar(
          ptrTypeId, spv::StorageClassOutput);
        
        m_module.decorateLocation(varId, e->registerId);
        m_module.setDebugName(varId, str::format("o", e->registerId).c_str());
        m_entryPointInterfaces.push_back(varId);
        
        m_ps.oregs.at(e->registerId).componentType  = e->componentType;
        m_ps.oregs.at(e->registerId).componentCount = e->componentMask.componentCount();
        m_ps.oregs.at(e->registerId).pointerId      = varId;
      }
    }
    
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
  
  
  void DxbcCompiler2::prepareVertexInputs() {
    // TODO implement
  }
  
  
  void DxbcCompiler2::preparePixelInputs() {
    // TODO implement
  }
  
  
  void DxbcCompiler2::prepareVertexOutputs() {
    for (const DxbcSvMapping2& svMapping : m_oSvs) {
      switch (svMapping.sv) {
        case DxbcSystemValue::Position: {
          DxbcPointer2 dstPtr;
          dstPtr.componentType  = DxbcScalarType::Float32;
          dstPtr.componentCount = 4;
          
          uint32_t regTypeId = this->defineVectorType(
            dstPtr.componentType, dstPtr.componentCount);
          
          uint32_t ptrTypeId = m_module.defPointerType(
            regTypeId, spv::StorageClassOutput);
          
          uint32_t memberId = m_module.constu32(PerVertex_Position);
          
          dstPtr.pointerId = m_module.opAccessChain(
            ptrTypeId, m_perVertexOut, 1, &memberId);
          
          DxbcPointer2 srcPtr;
          srcPtr.componentType  = DxbcScalarType::Float32;
          srcPtr.componentCount = 4;
          srcPtr.pointerId      = m_oRegs.at(svMapping.regId);
          
          this->storePtr(dstPtr, this->loadPtr(srcPtr),
            DxbcRegMask(true, true, true, true));
        } break;
        
        default:
          Logger::err(str::format(
            "dxbc: Unsupported vertex sv output: ",
            svMapping.sv));
      }
    }
  }
  
  
  void DxbcCompiler2::preparePixelOutputs() {
    // TODO implement
  }
  
  
  void DxbcCompiler2::endVertexShader() {
    this->prepareVertexInputs();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_vs.functionId, 0, nullptr);
    this->prepareVertexOutputs();
  }
  
  
  void DxbcCompiler2::endPixelShader() {
    this->preparePixelInputs();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_ps.functionId, 0, nullptr);
    this->preparePixelOutputs();
  }
  
  
  uint32_t DxbcCompiler2::definePerVertexBlock() {
    uint32_t t_f32    = m_module.defFloatType(32);
    uint32_t t_f32_v4 = m_module.defVectorType(t_f32, 4);
    uint32_t t_f32_a2 = m_module.defArrayType(t_f32, m_module.constu32(2));
    
    std::array<uint32_t, 4> members;
    members[PerVertex_Position]  = t_f32_v4;
    members[PerVertex_PointSize] = t_f32;
    members[PerVertex_CullDist]  = t_f32_a2;
    members[PerVertex_ClipDist]  = t_f32_a2;
    
    uint32_t typeId = m_module.defStructTypeUnique(
      members.size(), members.data());
    
    m_module.memberDecorateBuiltIn(typeId, PerVertex_Position, spv::BuiltInPosition);
    m_module.memberDecorateBuiltIn(typeId, PerVertex_PointSize, spv::BuiltInPointSize);
    m_module.memberDecorateBuiltIn(typeId, PerVertex_CullDist, spv::BuiltInCullDistance);
    m_module.memberDecorateBuiltIn(typeId, PerVertex_ClipDist, spv::BuiltInClipDistance);
    m_module.decorateBlock(typeId);
    
    m_module.setDebugName(typeId, "per_vertex");
    m_module.setDebugMemberName(typeId, PerVertex_Position, "position");
    m_module.setDebugMemberName(typeId, PerVertex_PointSize, "point_size");
    m_module.setDebugMemberName(typeId, PerVertex_CullDist, "cull_dist");
    m_module.setDebugMemberName(typeId, PerVertex_ClipDist, "clip_dist");
    return typeId;
  }
  
  
  uint32_t DxbcCompiler2::defineScalarType(
          DxbcScalarType        componentType) {
    switch (componentType) {
      case DxbcScalarType::Float32: return m_module.defFloatType(32);
      case DxbcScalarType::Float64: return m_module.defFloatType(64);
      case DxbcScalarType::Uint32:  return m_module.defIntType(32, 0);
      case DxbcScalarType::Uint64:  return m_module.defIntType(64, 0);
      case DxbcScalarType::Sint32:  return m_module.defIntType(32, 1);
      case DxbcScalarType::Sint64:  return m_module.defIntType(64, 1);
    }
    
    Logger::err("dxbc: Invalid scalar type");
    return 0;
  }
  
  
  uint32_t DxbcCompiler2::defineVectorType(
          DxbcScalarType        componentType,
          uint32_t              componentCount) {
    uint32_t typeId = this->defineScalarType(componentType);
    
    if (componentCount > 1) {
      typeId = m_module.defVectorType(
        typeId, componentCount);
    }
    
    return typeId;
  }
  
  
  uint32_t DxbcCompiler2::definePointerType(
          DxbcScalarType        componentType,
          uint32_t              componentCount,
          spv::StorageClass     storageClass) {
    return m_module.defPointerType(
      this->defineVectorType(
        componentType, componentCount),
      storageClass);
  }
  
  
  DxbcError DxbcCompiler2::parseInstruction(const DxbcInstruction& ins, DxbcInst& out) {
    out.opcode  = ins.token().opcode();
    out.control = ins.token().control();
    out.format  = dxbcInstructionFormat(out.opcode);
    
    // TODO implement extended opcodes
    
    uint32_t argOffset = 0;
    
    for (uint32_t i = 0; i < out.format.operandCount; i++) {
      
      if (out.format.operands[i].kind == DxbcOperandKind::Imm32) {
        // Some declarations use immediate DWORDs rather than the
        // immediate operand tokens, but we'll treat them the same.
        out.operands[i].type          = DxbcOperandType::Imm32;
        out.operands[i].immediates[0] = ins.arg(argOffset);
        argOffset += 1;
      } else {
        // Most instructions use proper operand tokens, which either
        // store an immediate value or references a register from one
        // of the indexable register files.
        const DxbcOperand op = ins.operand(argOffset);
        out.operands[i].type = op.token().type();
        
        // Parse operand modifiers
        DxbcOperandTokenExt token;
        
        if (op.queryOperandExt(DxbcOperandExt::OperandModifier, token))
          out.operands[i].modifiers = DxbcOperandModifiers(token.data());
        
        // Parse immediate values if applicable
        if (op.token().type() == DxbcOperandType::Imm32) {
          for (uint32_t j = 0; j < op.token().numComponents(); j++)
            out.operands[i].immediates[j] = op.imm32(j);
        }
        
        if (op.token().type() == DxbcOperandType::Imm64) {
          Logger::err("dxbc: 64-bit immediates not supported");
          return DxbcError::eInstructionFormat;
        }
        
        // Parse the indices. Each index is either a constant value
        // or the sum of a constant and a temporary register value.
        out.operands[i].indexDim = op.token().indexDimension();
        
        for (uint32_t j = 0; j < op.token().indexDimension(); j++) {
          const DxbcOperandIndex index = op.index(j);
          
          DxbcInstOpIndex& opIndex = out.operands[i].index[j];
          opIndex.type = index.hasRelPart()
            ? DxbcIndexType::Relative
            : DxbcIndexType::Immediate;
          opIndex.immediate = index.immPart();
          
          // If the index is relative, we have to parse another
          // operand token which must reference a r# register.
          if (index.hasRelPart()) {
            const DxbcOperand relPart = index.relPart();
            
            if (relPart.token().type() != DxbcOperandType::Temp) {
              Logger::err("dxbc: Invalid index register type");
              return DxbcError::eInstructionFormat;
            }
            
            if ((relPart.token().indexDimension() != 1)
             || (relPart.index(0).hasRelPart())) {
              Logger::err("dxbc: Invalid index register index");
              return DxbcError::eInstructionFormat;
            }
            
            if (relPart.token().selectionMode() != DxbcRegMode::Select1) {
              Logger::err("dxbc: Invalid index component selection mode");
              return DxbcError::eInstructionFormat;
            }
            
            // Assign the index and the component selection
            opIndex.tempRegId = relPart.index(0).immPart();
            opIndex.tempRegComponent = relPart.token().select1();
          }
        }
        
        // Parse component mask, swizzle or selection
        out.operands[i].componentCount = op.token().numComponents();
        out.operands[i].componentMode  = op.token().selectionMode();
        
        if (op.token().numComponents() == 4) {
          switch (op.token().selectionMode()) {
            case DxbcRegMode::Mask:
              out.operands[i].mask = op.token().mask();
              break;
              
            case DxbcRegMode::Swizzle:
              out.operands[i].swizzle = op.token().swizzle();
              break;
              
            case DxbcRegMode::Select1:
              out.operands[i].select1 = op.token().select1();
              break;
          }
        }
        
        // Move on to next operand
        argOffset += op.length();
      }
    }
    
    return DxbcError::sOk;
  }
  
}