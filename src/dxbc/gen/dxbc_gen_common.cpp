#include "dxbc_gen_common.h"
#include "dxbc_gen_vertex.h"

#include "../dxbc_names.h"

namespace dxvk {
  
  DxbcCodeGen::DxbcCodeGen() {
    m_module.enableCapability(spv::CapabilityShader);
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
      
      DxbcPointer reg;
      reg.type = DxbcPointerType(
        DxbcValueType(DxbcScalarType::Float32, 4),
        spv::StorageClassPrivate);
      
      const uint32_t typeId = this->defPointerType(reg.type);
      
      for (uint32_t i = oldSize; i < n; i++) {
        reg.valueId = m_module.newVar(typeId, spv::StorageClassPrivate);
        m_module.setDebugName(reg.valueId, str::format("r", i).c_str());
        m_rRegs.at(i) = reg;
      }
    }
  }
  
  
  Rc<DxbcCodeGen> DxbcCodeGen::create(
    const DxbcProgramVersion& version) {
    switch (version.type()) {
      case DxbcProgramType::VertexShader:
        return new DxbcVsCodeGen();
      
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
    
    uint32_t typeId = m_module.defStructType(
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
  
}