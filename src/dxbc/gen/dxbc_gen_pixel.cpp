#include "dxbc_gen_pixel.h"

namespace dxvk {
  
  DxbcPsCodeGen::DxbcPsCodeGen() {
    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityCullDistance);
    m_module.enableCapability(spv::CapabilityClipDistance);
    
    m_function = m_module.allocateId();
    m_module.setDebugName(m_function, "ps_main");
    
    m_module.functionBegin(
      m_module.defVoidType(),
      m_function,
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.opLabel(m_module.allocateId());
  }
  
  
  DxbcPsCodeGen::~DxbcPsCodeGen() {
    
  }
  
  
  void DxbcPsCodeGen::dclInterfaceVar(
          DxbcOperandType   regType,
          uint32_t          regId,
          uint32_t          regDim,
          DxbcComponentMask regMask,
          DxbcSystemValue   sv) {
    switch (regType) {
      case DxbcOperandType::Input: {
        if (m_vRegs.at(regId).valueId == 0) {
          m_vRegs.at(regId) = this->defVar(
            DxbcValueType(DxbcScalarType::Float32, 4),
            spv::StorageClassPrivate);
          m_module.setDebugName(m_vRegs.at(regId).valueId,
            str::format("v", regId).c_str());
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
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcPsCodeGen::dclInterfaceVar: Unhandled operand type: ",
          regType));
    }
  }
  
  
  DxbcPointer DxbcPsCodeGen::ptrInterfaceVar(
          DxbcOperandType   regType,
          uint32_t          regId) {
    switch (regType) {
      case DxbcOperandType::Input:
        return m_vRegs.at(regId);
      
      case DxbcOperandType::Output:
        return m_oRegs.at(regId);
      
      default:
        throw DxvkError(str::format(
          "DxbcPsCodeGen::ptrInterfaceVar: Unhandled operand type: ",
          regType));
    }
  }
  
  
  DxbcPointer DxbcPsCodeGen::ptrInterfaceVarIndexed(
          DxbcOperandType   regType,
          uint32_t          regId,
    const DxbcValue&        index) {
    throw DxvkError(str::format(
      "DxbcPsCodeGen::ptrInterfaceVarIndexed:\n",
      "Pixel shaders do not support indexed interface variables"));
  }
  
  
  Rc<DxvkShader> DxbcPsCodeGen::finalize() {
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
      spv::ExecutionModelFragment, "main",
      m_entryPointInterfaces.size(),
      m_entryPointInterfaces.data());
    m_module.setDebugName(m_entryPointId, "main");
    
    return new DxvkShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      m_module.compile());
  }
  
  
  void DxbcPsCodeGen::dclSvInputReg(DxbcSystemValue sv) {
    switch (sv) {
      case DxbcSystemValue::Position: {
        m_svPosition = this->defVar(
          DxbcValueType(DxbcScalarType::Float32, 4),
          spv::StorageClassInput);
        m_entryPointInterfaces.push_back(
          m_svPosition.valueId);
        
        m_module.setDebugName(m_svPosition.valueId, "sv_position");
        m_module.decorateBuiltIn(m_svPosition.valueId, spv::BuiltInFragCoord);
      } break;
      
      default:
        throw DxvkError(str::format(
          "DxbcPsCodeGen::dclSvInputReg: Unhandled SV: ", sv));
    }
  }
  
  
  void DxbcPsCodeGen::prepareSvInputs() {
    
  }
  
  
  void DxbcPsCodeGen::prepareSvOutputs() {
    
  }
  
}