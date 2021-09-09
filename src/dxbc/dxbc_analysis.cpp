#include "dxbc_analysis.h"

namespace dxvk {
  
  DxbcAnalyzer::DxbcAnalyzer(
    const DxbcModuleInfo&     moduleInfo,
    const DxbcProgramInfo&    programInfo,
    const Rc<DxbcIsgn>&       isgn,
    const Rc<DxbcIsgn>&       osgn,
    const Rc<DxbcIsgn>&       psgn,
          DxbcAnalysisInfo&   analysis)
  : m_isgn    (isgn),
    m_osgn    (osgn),
    m_psgn    (psgn),
    m_analysis(&analysis) {
    // Get number of clipping and culling planes from the
    // input and output signatures. We will need this to
    // declare the shader input and output interfaces.
    m_analysis->clipCullIn  = getClipCullInfo(m_isgn);
    m_analysis->clipCullOut = getClipCullInfo(m_osgn);
  }
  
  
  DxbcAnalyzer::~DxbcAnalyzer() {
    
  }
  
  
  void DxbcAnalyzer::processInstruction(const DxbcShaderInstruction& ins) {
    switch (ins.opClass) {
      case DxbcInstClass::Atomic: {
        const uint32_t operandId = ins.dstCount - 1;
        
        if (ins.dst[operandId].type == DxbcOperandType::UnorderedAccessView) {
          const uint32_t registerId = ins.dst[operandId].idx[0].offset;
          m_analysis->uavInfos[registerId].accessAtomicOp = true;
          m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        }
      } break;
      
      case DxbcInstClass::TextureSample:
      case DxbcInstClass::TextureGather:
      case DxbcInstClass::TextureQueryLod:
      case DxbcInstClass::VectorDeriv: {
        m_analysis->usesDerivatives = true;
      } break;
      
      case DxbcInstClass::ControlFlow: {
        if (ins.op == DxbcOpcode::Discard)
          m_analysis->usesKill = true;
      } break;
      
      case DxbcInstClass::BufferLoad: {
        uint32_t operandId = ins.op == DxbcOpcode::LdStructured ? 2 : 1;

        if (ins.src[operandId].type == DxbcOperandType::UnorderedAccessView) {
          const uint32_t registerId = ins.src[operandId].idx[0].offset;
          m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        }
      } break;
        
      case DxbcInstClass::BufferStore: {
        if (ins.dst[0].type == DxbcOperandType::UnorderedAccessView) {
          const uint32_t registerId = ins.dst[0].idx[0].offset;
          m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
        }
      } break;

      case DxbcInstClass::TypedUavLoad: {
        const uint32_t registerId = ins.src[1].idx[0].offset;
        m_analysis->uavInfos[registerId].accessTypedLoad = true;
        m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_READ_BIT;
      } break;
      
      case DxbcInstClass::TypedUavStore: {
        const uint32_t registerId = ins.dst[0].idx[0].offset;
        m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
      } break;
      
      default:
        return;
    }
  }
  
  
  DxbcClipCullInfo DxbcAnalyzer::getClipCullInfo(const Rc<DxbcIsgn>& sgn) const {
    DxbcClipCullInfo result;
    
    if (sgn != nullptr) {
      for (auto e = sgn->begin(); e != sgn->end(); e++) {
        const uint32_t componentCount = e->componentMask.popCount();
        
        if (e->systemValue == DxbcSystemValue::ClipDistance)
          result.numClipPlanes += componentCount;
        if (e->systemValue == DxbcSystemValue::CullDistance)
          result.numCullPlanes += componentCount;
      }
    }
    
    return result;
  }
  
}
