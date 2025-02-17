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

          // Check whether the atomic operation is order-invariant
          DxvkAccessOp store = DxvkAccessOp::None;

          switch (ins.op) {
            case DxbcOpcode::AtomicAnd:  store = DxvkAccessOp::And;  break;
            case DxbcOpcode::AtomicOr:   store = DxvkAccessOp::Or;   break;
            case DxbcOpcode::AtomicXor:  store = DxvkAccessOp::Xor;  break;
            case DxbcOpcode::AtomicIAdd: store = DxvkAccessOp::Add;  break;
            case DxbcOpcode::AtomicIMax: store = DxvkAccessOp::IMax; break;
            case DxbcOpcode::AtomicIMin: store = DxvkAccessOp::IMin; break;
            case DxbcOpcode::AtomicUMax: store = DxvkAccessOp::UMax; break;
            case DxbcOpcode::AtomicUMin: store = DxvkAccessOp::UMin; break;
            default: break;
          }

          if (m_analysis->uavInfos[registerId].atomicStore == DxvkAccessOp::None)
            m_analysis->uavInfos[registerId].atomicStore = store;

          // Maintain ordering if the UAV is accessed via other operations as well
          if (store == DxvkAccessOp::None || m_analysis->uavInfos[registerId].atomicStore != store)
            m_analysis->uavInfos[registerId].nonInvariantAccess = true;
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
        bool sparseFeedback = ins.dstCount == 2;

        if (ins.src[operandId].type == DxbcOperandType::UnorderedAccessView) {
          const uint32_t registerId = ins.src[operandId].idx[0].offset;
          m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_READ_BIT;
          m_analysis->uavInfos[registerId].sparseFeedback |= sparseFeedback;
          m_analysis->uavInfos[registerId].nonInvariantAccess = true;
        } else if (ins.src[operandId].type == DxbcOperandType::Resource) {
          const uint32_t registerId = ins.src[operandId].idx[0].offset;
          m_analysis->srvInfos[registerId].sparseFeedback |= sparseFeedback;
        }
      } break;

      case DxbcInstClass::BufferStore: {
        if (ins.dst[0].type == DxbcOperandType::UnorderedAccessView) {
          const uint32_t registerId = ins.dst[0].idx[0].offset;
          m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
          m_analysis->uavInfos[registerId].nonInvariantAccess = true;
        }
      } break;

      case DxbcInstClass::TypedUavLoad: {
        const uint32_t registerId = ins.src[1].idx[0].offset;
        m_analysis->uavInfos[registerId].accessTypedLoad = true;
        m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_READ_BIT;
        m_analysis->uavInfos[registerId].nonInvariantAccess = true;
      } break;
      
      case DxbcInstClass::TypedUavStore: {
        const uint32_t registerId = ins.dst[0].idx[0].offset;
        m_analysis->uavInfos[registerId].accessFlags |= VK_ACCESS_SHADER_WRITE_BIT;
        m_analysis->uavInfos[registerId].nonInvariantAccess = true;
      } break;

      case DxbcInstClass::Declaration: {
        switch (ins.op) {
          case DxbcOpcode::DclConstantBuffer: {
            uint32_t registerId = ins.dst[0].idx[0].offset;

            if (registerId < DxbcConstBufBindingCount)
              m_analysis->bindings.cbvMask |= 1u << registerId;
          } break;

          case DxbcOpcode::DclSampler: {
            uint32_t registerId = ins.dst[0].idx[0].offset;

            if (registerId < DxbcSamplerBindingCount)
              m_analysis->bindings.samplerMask |= 1u << registerId;
          } break;

          case DxbcOpcode::DclResource:
          case DxbcOpcode::DclResourceRaw:
          case DxbcOpcode::DclResourceStructured: {
            uint32_t registerId = ins.dst[0].idx[0].offset;

            uint32_t idx = registerId / 64u;
            uint32_t bit = registerId % 64u;

            if (registerId < DxbcResourceBindingCount)
              m_analysis->bindings.srvMask[idx] |= uint64_t(1u) << bit;
          } break;

          case DxbcOpcode::DclUavTyped:
          case DxbcOpcode::DclUavRaw:
          case DxbcOpcode::DclUavStructured: {
            uint32_t registerId = ins.dst[0].idx[0].offset;

            if (registerId < DxbcUavBindingCount)
              m_analysis->bindings.uavMask |= uint64_t(1u) << registerId;
          } break;

          default: ;
        }
      } break;

      default:
        break;
    }

    for (uint32_t i = 0; i < ins.dstCount; i++) {
      if (ins.dst[i].type == DxbcOperandType::IndexableTemp) {
        uint32_t index = ins.dst[i].idx[0].offset;
        m_analysis->xRegMasks[index] |= ins.dst[i].mask;
      }
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
