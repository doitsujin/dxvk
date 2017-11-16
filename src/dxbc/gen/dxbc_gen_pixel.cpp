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
        if (sv == DxbcSystemValue::None) {
          if (m_vRegs.at(regId).valueId == 0) {
            m_vRegs.at(regId) = this->defVar(
              DxbcValueType(DxbcScalarType::Float32, 4),
              spv::StorageClassInput);
            m_module.setDebugName(m_vRegs.at(regId).valueId,
              str::format("v", regId).c_str());
            m_module.decorateLocation(
              m_vRegs.at(regId).valueId, regId);
            m_entryPointInterfaces.push_back(
              m_vRegs.at(regId).valueId);
          }
        } else {
          if (m_vRegsSv.at(regId).valueId == 0) {
            m_vRegsSv.at(regId) = this->defVar(
              DxbcValueType(DxbcScalarType::Float32, 4),
              spv::StorageClassPrivate);
            m_module.setDebugName(m_vRegsSv.at(regId).valueId,
              str::format("sv", regId).c_str());
          }
        }
      } break;
      
      case DxbcOperandType::Output: {
        if (sv != DxbcSystemValue::None) {
          throw DxvkError(str::format(
            "DxbcPsCodeGen::dclInterfaceVar: Cannot map output register to system value: ",
            sv));
        }
        
        if (m_oRegs.at(regId).valueId == 0) {
          m_oRegs.at(regId) = this->defVar(
            DxbcValueType(DxbcScalarType::Float32, 4),
            spv::StorageClassOutput);
          m_module.setDebugName(m_oRegs.at(regId).valueId,
            str::format("o", regId).c_str());
          m_module.decorateLocation(
            m_oRegs.at(regId).valueId, regId);
          m_entryPointInterfaces.push_back(
            m_oRegs.at(regId).valueId);
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
        return m_vRegsSv.at(regId).valueId != 0
          ? m_vRegsSv.at(regId)
          : m_vRegs  .at(regId);
      
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
    
    return new DxvkShader(VK_SHADER_STAGE_FRAGMENT_BIT,
      m_module.compile(), 0, nullptr);
  }
  
  
  void DxbcPsCodeGen::prepareSvInputs() {
    
  }
  
  
  void DxbcPsCodeGen::prepareSvOutputs() {
    
  }
  
}