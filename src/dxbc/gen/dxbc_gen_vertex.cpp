#include "dxbc_gen_vertex.h"

namespace dxvk {
  
  DxbcVsCodeGen::DxbcVsCodeGen(const Rc<DxbcIsgn>& isgn) {
    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityCullDistance);
    m_module.enableCapability(spv::CapabilityClipDistance);
    
    m_function = m_module.allocateId();
    m_module.setDebugName(m_function, "vs_main");
    
    m_module.functionBegin(
      m_module.defVoidType(),
      m_function,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
    
    // Declare per-vertex builtin output block
    m_vsPerVertex = m_module.newVar(
      m_module.defPointerType(this->defPerVertexBlock(), spv::StorageClassOutput),
      spv::StorageClassOutput);
    m_entryPointInterfaces.push_back(m_vsPerVertex);
    m_module.setDebugName(m_vsPerVertex, "vs_per_vertex");
    
    // Declare per-vertex user output block
    m_vsOut = m_module.newVar(
      m_module.defPointerType(
        m_module.defArrayType(
          m_module.defVectorType(
            m_module.defFloatType(32), 4),
          m_module.constu32(32)),
        spv::StorageClassOutput),
      spv::StorageClassOutput);
    m_entryPointInterfaces.push_back(m_vsOut);
    m_module.decorateLocation(m_vsOut, 0);
    m_module.setDebugName(m_vsOut, "vs_out");
    
    // Declare vertex inputs based on the input signature
    for (auto e = isgn->begin(); e != isgn->end(); e++) {
      if (e->systemValue == DxbcSystemValue::None) {
        m_vsIn.at(e->registerId) = this->defVar(
          DxbcValueType(e->componentType, 4),
          spv::StorageClassInput);
        m_module.decorateLocation(
          m_vsIn.at(e->registerId).valueId,
          e->registerId);
        m_module.setDebugName(m_vsIn.at(e->registerId).valueId,
          str::format("vs_in", e->registerId).c_str());
        m_entryPointInterfaces.push_back(
          m_vsIn.at(e->registerId).valueId);
      }
    }
  }
  
  
  DxbcVsCodeGen::~DxbcVsCodeGen() {
    
  }
  
  
  void DxbcVsCodeGen::dclInterfaceVar(
          DxbcOperandType       regType,
          uint32_t              regId,
          uint32_t              regDim,
          DxbcComponentMask     regMask,
          DxbcSystemValue       sv,
          DxbcInterpolationMode im) {
    switch (regType) {
      case DxbcOperandType::Input: {
        if (m_vRegs.at(regId).valueId == 0) {
          m_vRegs.at(regId) = this->defVar(
            DxbcValueType(DxbcScalarType::Float32, 4),
            spv::StorageClassPrivate);
          m_module.setDebugName(m_vRegs.at(regId).valueId,
            str::format("v", regId).c_str());
        }
        
        if (sv != DxbcSystemValue::None) {
          m_svIn.push_back(DxbcSvMapping {
            regId, regMask, sv });
        }
      } break;
      
      case DxbcOperandType::Output: {
        if (m_oRegs.at(regId).valueId == 0) {
          m_oRegs.at(regId) = this->defVar(
            DxbcValueType(DxbcScalarType::Float32, 4),
            spv::StorageClassPrivate);
          m_module.setDebugName(m_oRegs.at(regId).valueId,
            str::format("o", regId).c_str());
        }
        
        if (sv != DxbcSystemValue::None) {
          m_svOut.push_back(DxbcSvMapping {
            regId, regMask, sv });
        }
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcVsCodeGen::dclInterfaceVar: Unhandled operand type: ",
          regType));
    }
  }
  
  
  DxbcPointer DxbcVsCodeGen::ptrInterfaceVar(
          DxbcOperandType       regType,
          uint32_t              regId) {
    switch (regType) {
      case DxbcOperandType::Input:
        return m_vRegs.at(regId);
      
      case DxbcOperandType::Output:
        return m_oRegs.at(regId);
        
      default:
        throw DxvkError(str::format(
          "DxbcVsCodeGen::ptrInterfaceVar: Unhandled operand type: ",
          regType));
    }
  }
  
  
  DxbcPointer DxbcVsCodeGen::ptrInterfaceVarIndexed(
          DxbcOperandType       regType,
          uint32_t              regId,
    const DxbcValue&            index) {
    throw DxvkError(str::format(
      "DxbcVsCodeGen::ptrInterfaceVarIndexed:\n",
      "Vertex shaders do not support indexed interface variables"));
  }
  
  
  SpirvCodeBuffer DxbcVsCodeGen::finalize() {
    m_module.functionBegin(
      m_module.defVoidType(),
      m_entryPointId,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
    
    this->prepareSvInputs();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_function, 0, nullptr);
    this->prepareSvOutputs();
    
    m_module.opReturn();
    m_module.functionEnd();
    
    m_module.addEntryPoint(m_entryPointId,
      spv::ExecutionModelVertex, "main",
      m_entryPointInterfaces.size(),
      m_entryPointInterfaces.data());
    m_module.setDebugName(m_entryPointId, "main");
    
    return m_module.compile();
  }
  
  
  void DxbcVsCodeGen::dclSvInputReg(DxbcSystemValue sv) {
    
  }
  
  
  void DxbcVsCodeGen::prepareSvInputs() {
    DxbcValueType targetType(DxbcScalarType::Float32, 4);
    
    // Copy vertex inputs to the actual shader input registers
    for (uint32_t i = 0; i < m_vsIn.size(); i++) {
      if ((m_vsIn.at(i).valueId != 0) && (m_vRegs.at(i).valueId != 0)) {
        DxbcValue srcValue = this->regLoad(m_vsIn.at(i));
                  srcValue = this->regCast(srcValue, targetType);
        this->regStore(m_vRegs.at(i), srcValue,
          DxbcComponentMask(true, true, true, true));
      }
    }
    
    // TODO system values
  }
  
  
  void DxbcVsCodeGen::prepareSvOutputs() {
    for (uint32_t i = 0; i < m_oRegs.size(); i++) {
      if (m_oRegs.at(i).valueId != 0) {
        this->regStore(
          this->getVsOutPtr(i),
          this->regLoad(m_oRegs.at(i)),
          DxbcComponentMask(true, true, true, true));
      }
    }
    
    for (const auto& mapping : m_svOut) {
      DxbcValue srcValue = this->regLoad(m_oRegs.at(mapping.regId));
      
      switch (mapping.sv) {
        case DxbcSystemValue::Position: {
          this->regStore(this->ptrBuiltInPosition(), srcValue,
            DxbcComponentMask(true, true, true, true));
        } break;
      }
    }
  }
  
  
  DxbcPointer DxbcVsCodeGen::ptrBuiltInPosition() {
    const uint32_t memberId = m_module.constu32(PerVertex_Position);
    
    DxbcPointer result;
    result.type = DxbcPointerType(
      DxbcValueType(DxbcScalarType::Float32, 4),
      spv::StorageClassOutput);
    result.valueId = m_module.opAccessChain(
      this->defPointerType(result.type),
      m_vsPerVertex, 1, &memberId);
    return result;
  }
  
  
  DxbcPointer DxbcVsCodeGen::getVsOutPtr(uint32_t id) {
    const uint32_t memberId = m_module.constu32(id);
    
    DxbcPointer result;
    result.type = DxbcPointerType(
      DxbcValueType(DxbcScalarType::Float32, 4),
      spv::StorageClassOutput);
    result.valueId = m_module.opAccessChain(
      this->defPointerType(result.type),
      m_vsOut, 1, &memberId);
    return result;
  }
  
}