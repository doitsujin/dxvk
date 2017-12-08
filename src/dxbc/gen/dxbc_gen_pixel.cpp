#include "dxbc_gen_pixel.h"

namespace dxvk {
  
  DxbcPsCodeGen::DxbcPsCodeGen(
    const Rc<DxbcIsgn>& osgn) {
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
    
    // Declare outputs based on the input signature
    for (auto e = osgn->begin(); e != osgn->end(); e++) {
      if (e->systemValue == DxbcSystemValue::None) {
        const DxbcPointer var = this->defVar(
          DxbcValueType(e->componentType, e->componentMask.componentCount()),
          spv::StorageClassOutput);
        
        m_psOut.at(e->registerId) = var;
        
        m_module.decorateLocation(var.valueId, e->registerId);
        m_module.setDebugName(var.valueId,
          str::format("ps_out", e->registerId).c_str());
        m_entryPointInterfaces.push_back(var.valueId);
      }
    }
  }
  
  
  DxbcPsCodeGen::~DxbcPsCodeGen() {
    
  }
  
  
  void DxbcPsCodeGen::dclInterfaceVar(
          DxbcOperandType       regType,
          uint32_t              regId,
          uint32_t              regDim,
          DxbcComponentMask     regMask,
          DxbcSystemValue       sv,
          DxbcInterpolationMode im) {
    switch (regType) {
      case DxbcOperandType::Input: {
        if (m_vRegs.at(regId).valueId == 0) {
          const DxbcPointer var = this->defVar(
            DxbcValueType(DxbcScalarType::Float32, 4),
            spv::StorageClassInput);
          
          m_vRegs.at(regId) = var;
          m_module.decorateLocation(var.valueId, regId);
          m_module.setDebugName(var.valueId,
            str::format("v", regId).c_str());
          
          switch (im) {
            case DxbcInterpolationMode::Undefined:
            case DxbcInterpolationMode::Linear:
              break;
              
            case DxbcInterpolationMode::Constant:
              m_module.decorate(var.valueId, spv::DecorationFlat);
              break;
              
            case DxbcInterpolationMode::LinearCentroid:
              m_module.decorate(var.valueId, spv::DecorationCentroid);
              break;
              
            case DxbcInterpolationMode::LinearNoPerspective:
              m_module.decorate(var.valueId, spv::DecorationNoPerspective);
              break;
              
            case DxbcInterpolationMode::LinearNoPerspectiveCentroid:
              m_module.decorate(var.valueId, spv::DecorationNoPerspective);
              m_module.decorate(var.valueId, spv::DecorationCentroid);
              break;
              
            case DxbcInterpolationMode::LinearSample:
              m_module.decorate(var.valueId, spv::DecorationSample);
              break;
              
            case DxbcInterpolationMode::LinearNoPerspectiveSample:
              m_module.decorate(var.valueId, spv::DecorationNoPerspective);
              m_module.decorate(var.valueId, spv::DecorationSample);
              break;
          }
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
          DxbcOperandType       regType,
          uint32_t              regId) {
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
          DxbcOperandType       regType,
          uint32_t              regId,
    const DxbcValue&            index) {
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
    m_module.setOriginUpperLeft(m_entryPointId);
    m_module.setDebugName(m_entryPointId, "main");
    
    return new DxvkShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0, nullptr,
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
    // TODO properly re-implement this
    std::array<uint32_t, 5> masks = { 0x0, 0x1, 0x3, 0x7, 0xF };
    
    for (uint32_t i = 0; i < m_psOut.size(); i++) {
      if ((m_psOut.at(i).valueId != 0) && (m_oRegs.at(i).valueId != 0)) {
        DxbcValue srcValue = this->regLoad(m_oRegs.at(i));
                  srcValue = this->regCast(srcValue, m_psOut.at(i).type.valueType);
        this->regStore(m_psOut.at(i), srcValue, DxbcComponentMask(
          masks.at(m_psOut.at(i).type.valueType.componentCount)));
      }
    }
  }
  
}