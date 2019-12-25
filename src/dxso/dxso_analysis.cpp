#include "dxso_analysis.h"

namespace dxvk {

  DxsoAnalyzer::DxsoAnalyzer(
    DxsoAnalysisInfo& analysis)
    : m_analysis(&analysis) { }

  void DxsoAnalyzer::processInstruction(
    const DxsoInstructionContext& ctx) {
    DxsoOpcode opcode = ctx.instruction.opcode;

    // Co-issued CNDs are issued before their parents,
    // except when the parent is a CND.
    if (opcode == DxsoOpcode::Cnd &&
        m_parentOpcode != DxsoOpcode::Cnd &&
        ctx.instruction.coissue) {
      m_analysis->coissues.push_back(ctx);
    }

    if (opcode == DxsoOpcode::TexKill)
      m_analysis->usesKill = true;

    if (opcode == DxsoOpcode::DsX
     || opcode == DxsoOpcode::DsY

     || opcode == DxsoOpcode::Tex
     || opcode == DxsoOpcode::TexCoord
     || opcode == DxsoOpcode::TexBem
     || opcode == DxsoOpcode::TexBemL
     || opcode == DxsoOpcode::TexReg2Ar
     || opcode == DxsoOpcode::TexReg2Gb
     || opcode == DxsoOpcode::TexM3x2Pad
     || opcode == DxsoOpcode::TexM3x2Tex
     || opcode == DxsoOpcode::TexM3x3Pad
     || opcode == DxsoOpcode::TexM3x3Tex
     || opcode == DxsoOpcode::TexM3x3Spec
     || opcode == DxsoOpcode::TexM3x3VSpec
     || opcode == DxsoOpcode::TexReg2Rgb
     || opcode == DxsoOpcode::TexDp3Tex
     || opcode == DxsoOpcode::TexM3x2Depth
     || opcode == DxsoOpcode::TexDp3
     || opcode == DxsoOpcode::TexM3x3
     //  Explicit LOD.
     //|| opcode == DxsoOpcode::TexLdd
     //|| opcode == DxsoOpcode::TexLdl
     || opcode == DxsoOpcode::TexDepth)
      m_analysis->usesDerivatives = true;

    m_parentOpcode = ctx.instruction.opcode;
  }

  void DxsoAnalyzer::finalize(size_t tokenCount) {
    m_analysis->bytecodeByteLength = tokenCount * sizeof(uint32_t);
  }

}