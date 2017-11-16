#include "dxbc_gen_vertex.h"

namespace dxvk {
  
  DxbcVsCodeGen::DxbcVsCodeGen() {
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
    
    m_outPerVertex = m_module.newVar(
      m_module.defPointerType(this->defPerVertexBlock(), spv::StorageClassOutput),
      spv::StorageClassOutput);
    m_entryPointInterfaces.push_back(m_outPerVertex);
    m_module.setDebugName(m_outPerVertex, "vs_out");
  }
  
  
  DxbcVsCodeGen::~DxbcVsCodeGen() {
    
  }
  
  
  void DxbcVsCodeGen::dclInterfaceVar(
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
        
        if (sv != DxbcSystemValue::None) {
          m_svOutputs.push_back(DxbcSvMapping {
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
          "DxbcVsCodeGen::ptrInterfaceVar: Unhandled operand type: ",
          regType));
    }
  }
  
  
  DxbcPointer DxbcVsCodeGen::ptrInterfaceVarIndexed(
          DxbcOperandType   regType,
          uint32_t          regId,
    const DxbcValue&        index) {
    throw DxvkError(str::format(
      "DxbcVsCodeGen::ptrInterfaceVarIndexed:\n",
      "Vertex shaders do not support indexed interface variables"));
  }
  
  
  Rc<DxvkShader> DxbcVsCodeGen::finalize() {
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
    
    return new DxvkShader(VK_SHADER_STAGE_VERTEX_BIT,
      m_module.compile(), 0, nullptr);
  }
  
  
  void DxbcVsCodeGen::prepareSvInputs() {
    // TODO implement
  }
  
  
  void DxbcVsCodeGen::prepareSvOutputs() {
    for (const auto& sv : m_svOutputs) {
      DxbcValue val = this->regLoad(m_oRegs.at(sv.regId));
//                 val = this->regExtract(val, sv.regMask);
      
      DxbcPointer dst;
      
      switch (sv.sv) {
        case DxbcSystemValue::Position:
          dst = this->ptrBuiltInPosition();
          break;
        
        default:
          Logger::err(str::format(
            "DxbcVsCodeGen::prepareSvOutputs: Unsupported system value: ",
            sv.sv));
      }
      
      if (dst.valueId != 0) {
//         val = this->regCast(val, dst.type.valueType);
        this->regStore(dst, val, DxbcComponentMask());
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
      m_outPerVertex, 1, &memberId);
    return result;
  }
  
}