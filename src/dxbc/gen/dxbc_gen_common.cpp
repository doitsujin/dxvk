#include "dxbc_gen_common.h"
#include "dxbc_gen_pixel.h"
#include "dxbc_gen_vertex.h"

#include "../dxbc_names.h"

namespace dxvk {
  
  DxbcCodeGen::DxbcCodeGen(DxbcProgramType shaderStage)
  : m_shaderStage(shaderStage) {
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);
    m_entryPointId = m_module.allocateId();
  }
  
  
  DxbcCodeGen::~DxbcCodeGen() {
    
  }
  
  
  void DxbcCodeGen::dclTemps(uint32_t n) {
    const uint32_t oldSize = m_rRegs.size();
    
    if (n > oldSize) {
      m_rRegs.resize(n);
      
      for (uint32_t i = oldSize; i < n; i++) {
        m_rRegs.at(i) = this->defVar(
          DxbcValueType(DxbcScalarType::Float32, 4),
          spv::StorageClassPrivate);
        m_module.setDebugName(m_rRegs.at(i).valueId,
          str::format("r", i).c_str());
      }
    }
  }
  
  
  void DxbcCodeGen::dclConstantBuffer(
          uint32_t              bufferId,
          uint32_t              elementCount) {
    // Uniform buffer data is stored as a fixed-size array
    // of 4x32-bit vectors. SPIR-V requires explicit strides.
    uint32_t arrayType = m_module.defArrayTypeUnique(
      this->defValueType(DxbcValueType(DxbcScalarType::Float32, 4)),
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
    uint32_t bindingId = computeResourceSlotId(m_shaderStage,
      DxbcBindingType::ConstantBuffer, bufferId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    m_resourceSlots.push_back(resource);
  }
  
  
  void DxbcCodeGen::dclResource(
          uint32_t              registerId,
          DxbcResourceDim       resourceType,
          DxbcResourceReturnType returnType) {
    uint32_t sampledTypeId = 0;
    
    switch (returnType) {
      default: Logger::err(str::format("DXBC: Invalid sampled type: ", returnType));
      case DxbcResourceReturnType::Float: sampledTypeId = m_module.defFloatType(32);    break;
      case DxbcResourceReturnType::Sint:  sampledTypeId = m_module.defIntType  (32, 1); break;
      case DxbcResourceReturnType::Uint:  sampledTypeId = m_module.defIntType  (32, 0); break;
    }
    
    uint32_t resourceTypeId = 0;
    
    switch (resourceType) {
      default:
        Logger::err(str::format(
          "DXBC: Invalid resource type: ",
          resourceType));
        
      case DxbcResourceDim::Texture1D:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim1D, 2, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture1DArr:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim1D, 2, 1, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture2D:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim2D, 2, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture2DArr:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim2D, 2, 1, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::Texture3D:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::Dim3D, 2, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::TextureCube:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::DimCube, 2, 0, 0, 1,
          spv::ImageFormatUnknown);
        break;
      
      case DxbcResourceDim::TextureCubeArr:
        resourceTypeId = m_module.defImageType(
          sampledTypeId, spv::DimCube, 2, 1, 0, 1,
          spv::ImageFormatUnknown);
        break;
    }
    
    uint32_t resourcePtrType = m_module.defPointerType(
      resourceTypeId, spv::StorageClassUniformConstant);
    
    uint32_t varId = m_module.newVar(resourcePtrType,
      spv::StorageClassUniformConstant);
    
    m_module.setDebugName(varId,
      str::format("t", registerId).c_str());
    
    m_resources.at(registerId).varId          = varId;
    m_resources.at(registerId).sampledTypeId  = sampledTypeId;
    m_resources.at(registerId).resourceTypeId = resourceTypeId;
    
    // Compute the DXVK binding slot index for the resource.
    // D3D11 needs to bind the actual resource to this slot.
    uint32_t bindingId = computeResourceSlotId(m_shaderStage,
      DxbcBindingType::ShaderResource, registerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    m_resourceSlots.push_back(resource);
  }
  
  
  void DxbcCodeGen::dclSampler(uint32_t samplerId) {
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
    
    m_samplers.at(samplerId).varId = varId;
    
    // Compute binding slot index for the sampler
    uint32_t bindingId = computeResourceSlotId(m_shaderStage,
      DxbcBindingType::ImageSampler, samplerId);
    
    m_module.decorateDescriptorSet(varId, 0);
    m_module.decorateBinding(varId, bindingId);
    
    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot = bindingId;
    resource.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    m_resourceSlots.push_back(resource);
  }
  
  
  DxbcValue DxbcCodeGen::defConstScalar(uint32_t v) {
    DxbcValue result;
    result.type    = DxbcValueType(DxbcScalarType::Uint32, 1);
    result.valueId = m_module.constu32(v);
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::defConstVector(
          uint32_t x, uint32_t y,
          uint32_t z, uint32_t w) {
    std::array<uint32_t, 4> ids = {
      m_module.constu32(x),
      m_module.constu32(y),
      m_module.constu32(z),
      m_module.constu32(w) };
    
    DxbcValue result;
    result.type    = DxbcValueType(DxbcScalarType::Uint32, 4);
    result.valueId = m_module.constComposite(
      this->defValueType(result.type), 
      ids.size(), ids.data());
    return result;
  }
  
  
  void DxbcCodeGen::fnReturn() {
    // TODO implement control flow
    m_module.opReturn();
    m_module.functionEnd();
  }
  
  
  DxbcPointer DxbcCodeGen::ptrTempReg(uint32_t regId) {
    return m_rRegs.at(regId);
  }
  
  
  DxbcPointer DxbcCodeGen::ptrConstantBuffer(
          uint32_t              regId,
    const DxbcValue&            index) {
    // The first index selects the struct member,
    // the second one selects the array element.
    std::array<uint32_t, 2> indices = { 
      m_module.constu32(0), index.valueId };
    
    DxbcPointer result;
    result.type = DxbcPointerType(
      DxbcValueType(DxbcScalarType::Float32, 4),
      spv::StorageClassUniform);
    result.valueId = m_module.opAccessChain(
      this->defPointerType(result.type),
      m_constantBuffers.at(regId).varId,
      2, indices.data());
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::opAbs(const DxbcValue& src) {
    DxbcValue result;
    result.type = src.type;
    
    switch (src.type.componentType) {
      case DxbcScalarType::Sint32:
      case DxbcScalarType::Sint64:
        result.valueId = m_module.opSAbs(
          this->defValueType(result.type),
          src.valueId);
        break;
        
      case DxbcScalarType::Uint32:
      case DxbcScalarType::Uint64:
        result.valueId = src.valueId;
        break;
      
      case DxbcScalarType::Float32:
      case DxbcScalarType::Float64:
        result.valueId = m_module.opFAbs(
          this->defValueType(result.type),
          src.valueId);
        break;
    }
    
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::opAdd(const DxbcValue& a, const DxbcValue& b) {
    DxbcValue result;
    result.type = a.type;
    
    switch (result.type.componentType) {
      case DxbcScalarType::Sint32:
      case DxbcScalarType::Sint64:
      case DxbcScalarType::Uint32:
      case DxbcScalarType::Uint64:
        result.valueId = m_module.opIAdd(
          this->defValueType(result.type),
          a.valueId, b.valueId);
        break;
      
      case DxbcScalarType::Float32:
      case DxbcScalarType::Float64:
        result.valueId = m_module.opFAdd(
          this->defValueType(result.type),
          a.valueId, b.valueId);
        break;
    }
    
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::opMul(const DxbcValue& a, const DxbcValue& b) {
    DxbcValue result;
    result.type = a.type;
    
    switch (result.type.componentType) {
      case DxbcScalarType::Sint32:
      case DxbcScalarType::Sint64:
      case DxbcScalarType::Uint32:
      case DxbcScalarType::Uint64:
        result.valueId = m_module.opIMul(
          this->defValueType(result.type),
          a.valueId, b.valueId);
        break;
      
      case DxbcScalarType::Float32:
      case DxbcScalarType::Float64:
        result.valueId = m_module.opFMul(
          this->defValueType(result.type),
          a.valueId, b.valueId);
        break;
    }
    
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::opDot(const DxbcValue& a, const DxbcValue& b) {
    DxbcValue result;
    result.type    = DxbcValueType(a.type.componentType, 1);
    result.valueId = m_module.opDot(
      this->defValueType(result.type),
      a.valueId, b.valueId);
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::opNeg(const DxbcValue& src) {
    DxbcValue result;
    result.type = src.type;
    
    switch (src.type.componentType) {
      case DxbcScalarType::Sint32:
      case DxbcScalarType::Sint64:
      case DxbcScalarType::Uint32:
      case DxbcScalarType::Uint64:
        result.valueId = m_module.opSNegate(
          this->defValueType(result.type),
          src.valueId);
        break;
      
      case DxbcScalarType::Float32:
      case DxbcScalarType::Float64:
        result.valueId = m_module.opFNegate(
          this->defValueType(result.type),
          src.valueId);
        break;
    }
    
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::opSaturate(const DxbcValue& src) {
    const uint32_t typeId = this->defValueType(src.type);
    
    std::array<uint32_t, 4> const0;
    std::array<uint32_t, 4> const1;
    
    uint32_t const0Id = 0;
    uint32_t const1Id = 0;
    
    if (src.type.componentType == DxbcScalarType::Float32) {
      const0Id = m_module.constf32(0.0f);
      const1Id = m_module.constf32(1.0f);
    } else if (src.type.componentType == DxbcScalarType::Float64) {
      const0Id = m_module.constf64(0.0);
      const1Id = m_module.constf64(1.0);
    } 
    
    for (uint32_t i = 0; i < src.type.componentCount; i++) {
      const0.at(i) = const0Id;
      const1.at(i) = const1Id;
    }
    
    if (src.type.componentCount > 1) {
      const0Id = m_module.constComposite(typeId, src.type.componentCount, const0.data());
      const1Id = m_module.constComposite(typeId, src.type.componentCount, const1.data());
    }
    
    DxbcValue result;
    result.type    = src.type;
    result.valueId = m_module.opFClamp(
      typeId, src.valueId, const0Id, const1Id);
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::regCast(
    const DxbcValue&            src,
    const DxbcValueType&        type) {
    if (src.type.componentType == type.componentType)
      return src;
    
    DxbcValue result;
    result.type    = type;
    result.valueId = m_module.opBitcast(
      this->defValueType(result.type),
      src.valueId);
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::regExtract(
    const DxbcValue&            src,
          DxbcComponentMask     mask) {
    return this->regSwizzle(src,
      DxbcComponentSwizzle(), mask);
  }
  
  
  DxbcValue DxbcCodeGen::regSwizzle(
    const DxbcValue&            src,
    const DxbcComponentSwizzle& swizzle,
          DxbcComponentMask     mask) {
    std::array<uint32_t, 4> indices;
    
    uint32_t dstIndex = 0;
    for (uint32_t i = 0; i < src.type.componentCount; i++) {
      if (mask.test(i))
        indices[dstIndex++] = swizzle[i];
    }
    
    // If the swizzle combined with the mask can be reduced
    // to a no-op, we don't need to insert any instructions.
    bool isIdentitySwizzle = dstIndex == src.type.componentCount;
    
    for (uint32_t i = 0; i < dstIndex && isIdentitySwizzle; i++)
      isIdentitySwizzle &= indices[i] == i;
    
    if (isIdentitySwizzle)
      return src;
    
    // Use OpCompositeExtract if the resulting vector contains
    // only one component, and OpVectorShuffle if it is a vector.
    DxbcValue result;
    result.type = DxbcValueType(src.type.componentType, dstIndex);
    
    if (dstIndex == 1) {
      result.valueId = m_module.opCompositeExtract(
        this->defValueType(result.type),
        src.valueId, 1, indices.data());
    } else {
      result.valueId = m_module.opVectorShuffle(
        this->defValueType(result.type),
        src.valueId, src.valueId,
        dstIndex, indices.data());
    }
    
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::regInsert(
    const DxbcValue&            dst,
    const DxbcValue&            src,
          DxbcComponentMask     mask) {
    DxbcValue result;
    result.type = dst.type;
    
    if (dst.type.componentCount == 1) {
      // Both values are scalar, so the first component
      // of the write mask decides which one to take.
      result.valueId = mask.test(0)
        ? src.valueId : dst.valueId;
    } else if (src.type.componentCount == 1) {
      // The source value is scalar. Since OpVectorShuffle
      // requires both arguments to be vectors, we have to
      // use OpCompositeInsert to modify the vector instead.
      const uint32_t componentId = mask.firstComponent();
      
      result.valueId = m_module.opCompositeInsert(
        this->defValueType(result.type),
        src.valueId, dst.valueId,
        1, &componentId);
    } else {
      // Both arguments are vectors. We can determine which
      // components to take from which vector and use the
      // OpVectorShuffle instruction.
      std::array<uint32_t, 4> components;
      uint32_t srcComponentId = dst.type.componentCount;
      
      for (uint32_t i = 0; i < dst.type.componentCount; i++)
        components[i] = mask.test(i) ? srcComponentId++ : i;
      
      result.valueId = m_module.opVectorShuffle(
        this->defValueType(result.type),
        dst.valueId, src.valueId,
        dst.type.componentCount,
        components.data());
    }
    
    return result;
  }
  
  
  DxbcValue DxbcCodeGen::regLoad(const DxbcPointer& ptr) {
    DxbcValue result;
    result.type    = ptr.type.valueType;
    result.valueId = m_module.opLoad(
      this->defValueType(result.type),
      ptr.valueId);
    return result;
  }
  
  
  void DxbcCodeGen::regStore(
    const DxbcPointer&          ptr,
    const DxbcValue&            val,
          DxbcComponentMask     mask) {
    if (ptr.type.valueType.componentCount != val.type.componentCount) {
      // In case we write to only a part of the destination
      // register, we need to load the previous value first
      // and then update the given components.
      DxbcValue tmp = this->regLoad(ptr);
                tmp = this->regInsert(tmp, val, mask);
                
      m_module.opStore(ptr.valueId, tmp.valueId);
    } else {
      // All destination components get written, so we don't
      // need to load and modify the target register first.
      m_module.opStore(ptr.valueId, val.valueId);
    }
  }
  
  
  Rc<DxbcCodeGen> DxbcCodeGen::create(
    const DxbcProgramVersion& version,
    const Rc<DxbcIsgn>&       isgn,
    const Rc<DxbcIsgn>&       osgn) {
    switch (version.type()) {
      case DxbcProgramType::PixelShader:
        return new DxbcPsCodeGen(osgn);
      
      case DxbcProgramType::VertexShader:
        return new DxbcVsCodeGen(isgn);
      
      default:
        throw DxvkError(str::format(
          "DxbcCodeGen::create: Unsupported program type: ",
          version.type()));
    }
  }
  
  
  uint32_t DxbcCodeGen::defScalarType(DxbcScalarType type) {
    switch (type) {
      case DxbcScalarType::Uint32 : return m_module.defIntType(32, 0);
      case DxbcScalarType::Uint64 : return m_module.defIntType(64, 0);
      case DxbcScalarType::Sint32 : return m_module.defIntType(32, 1);
      case DxbcScalarType::Sint64 : return m_module.defIntType(64, 1);
      case DxbcScalarType::Float32: return m_module.defFloatType(32);
      case DxbcScalarType::Float64: return m_module.defFloatType(64);
      
      default:
        throw DxvkError("DxbcCodeGen::defScalarType: Invalid scalar type");
    }
  }
  
  
  uint32_t DxbcCodeGen::defValueType(const DxbcValueType& type) {
    uint32_t typeId = this->defScalarType(type.componentType);
    
    if (type.componentCount > 1)
      typeId = m_module.defVectorType(typeId, type.componentCount);
    
    if (type.elementCount > 0)
      typeId = m_module.defArrayType(typeId, m_module.constu32(type.elementCount));
    
    return typeId;
  }
  
  
  uint32_t DxbcCodeGen::defPointerType(const DxbcPointerType& type) {
    uint32_t valueTypeId = this->defValueType(type.valueType);
    return m_module.defPointerType(valueTypeId, type.storageClass);
  }
  
  
  uint32_t DxbcCodeGen::defPerVertexBlock() {
    uint32_t s1f32 = this->defScalarType(DxbcScalarType::Float32);
    uint32_t v4f32 = this->defValueType(DxbcValueType(DxbcScalarType::Float32, 4, 0));
    uint32_t a2f32 = this->defValueType(DxbcValueType(DxbcScalarType::Float32, 1, 2));
    
    std::array<uint32_t, 4> members;
    members[PerVertex_Position]  = v4f32;
    members[PerVertex_PointSize] = s1f32;
    members[PerVertex_CullDist]  = a2f32;
    members[PerVertex_ClipDist]  = a2f32;
    
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
  
  
  DxbcPointer DxbcCodeGen::defVar(
    const DxbcValueType&          type,
          spv::StorageClass       storageClass) {
    DxbcPointer result;
    result.type    = DxbcPointerType(type, storageClass);
    result.valueId = m_module.newVar(
      this->defPointerType(result.type),
      storageClass);
    return result;
  }
  
}