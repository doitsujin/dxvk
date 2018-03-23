#include "dxbc_analysis.h"

namespace dxvk {
  
  DxbcAnalyzer::DxbcAnalyzer(
    const DxbcOptions&        options,
    const DxbcProgramVersion& version,
          DxbcAnalysisInfo&   analysis)
  : m_analysis(&analysis) {
    
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
        }
      } break;
        
      case DxbcInstClass::TypedUavLoad: {
        const uint32_t registerId = ins.src[1].idx[0].offset;
        m_analysis->uavInfos[registerId].accessTypedLoad = true;
      } break;
      
      default:
        return;
    }
  }
  
}