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
            spv::StorageClassInput);
          m_module.decorateLocation(m_vRegs.at(regId).valueId, regId);
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
            spv::StorageClassOutput);
          m_module.decorateLocation(m_oRegs.at(regId).valueId, regId);
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
    
    // TODO system values
  }
  
  
  void DxbcVsCodeGen::prepareSvOutputs() {
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
  
}