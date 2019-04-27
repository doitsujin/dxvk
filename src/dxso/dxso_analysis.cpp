#include "dxso_analysis.h"

namespace dxvk {

  DxsoAnalyzer::DxsoAnalyzer(
    DxsoAnalysisInfo& analysis)
    : m_analysis(&analysis) { }

  void DxsoAnalyzer::processInstruction(
    const DxsoInstructionContext& ctx) {
    DxsoOpcode opcode = ctx.instruction.opcode;

    if (opcode == DxsoOpcode::TexKill)
      m_analysis->usesKill = true;

    if (opcode == DxsoOpcode::DsX
     || opcode == DxsoOpcode::DsY)
      m_analysis->usesDerivatives = true;
  }

  void DxsoAnalyzer::finalize(size_t tokenCount) {
    m_analysis->bytecodeByteLength = tokenCount * sizeof(uint32_t);
  }

}