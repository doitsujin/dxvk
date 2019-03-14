#include "dxso_enums.h"

namespace dxvk {

  std::ostream& operator << (std::ostream& os, DxsoOpcode opcode) {
    switch (opcode) {
      case DxsoOpcode::Nop: os << "Nop"; break;
      case DxsoOpcode::Mov: os << "Mov"; break;
      case DxsoOpcode::Add: os << "Add"; break;
      case DxsoOpcode::Sub: os << "Sub"; break;
      case DxsoOpcode::Mad: os << "Mad"; break;
      case DxsoOpcode::Mul: os << "Mul"; break;
      case DxsoOpcode::Rcp: os << "Rcp"; break;
      case DxsoOpcode::Rsq: os << "Rsq"; break;
      case DxsoOpcode::Dp3: os << "Dp3"; break;
      case DxsoOpcode::Dp4: os << "Dp4"; break;
      case DxsoOpcode::Min: os << "Min"; break;
      case DxsoOpcode::Max: os << "Max"; break;
      case DxsoOpcode::Slt: os << "Slt"; break;
      case DxsoOpcode::Sge: os << "Sge"; break;
      case DxsoOpcode::Exp: os << "Exp"; break;
      case DxsoOpcode::Log: os << "Log"; break;
      case DxsoOpcode::Lit: os << "Lit"; break;
      case DxsoOpcode::Dst: os << "Dst"; break;
      case DxsoOpcode::Lrp: os << "Lrp"; break;
      case DxsoOpcode::Frc: os << "Frc"; break;
      case DxsoOpcode::M4x4: os << "M4x4"; break;
      case DxsoOpcode::M4x3: os << "M4x3"; break;
      case DxsoOpcode::M3x4: os << "M3x4"; break;
      case DxsoOpcode::M3x3: os << "M3x3"; break;
      case DxsoOpcode::M3x2: os << "M3x2"; break;
      case DxsoOpcode::Call: os << "Call"; break;
      case DxsoOpcode::CallNz: os << "CallNz"; break;
      case DxsoOpcode::Loop: os << "Loop"; break;
      case DxsoOpcode::Ret: os << "Ret"; break;
      case DxsoOpcode::EndLoop: os << "EndLoop"; break;
      case DxsoOpcode::Label: os << "Label"; break;
      case DxsoOpcode::Dcl: os << "Dcl"; break;
      case DxsoOpcode::Pow: os << "Pow"; break;
      case DxsoOpcode::Crs: os << "Crs"; break;
      case DxsoOpcode::Sgn: os << "Sgn"; break;
      case DxsoOpcode::Abs: os << "Abs"; break;
      case DxsoOpcode::Nrm: os << "Nrm"; break;
      case DxsoOpcode::SinCos: os << "SinCos"; break;
      case DxsoOpcode::Rep: os << "Rep"; break;
      case DxsoOpcode::EndRep: os << "EndRep"; break;
      case DxsoOpcode::If: os << "If"; break;
      case DxsoOpcode::Ifc: os << "Ifc"; break;
      case DxsoOpcode::Else: os << "Else"; break;
      case DxsoOpcode::EndIf: os << "EndIf"; break;
      case DxsoOpcode::Break: os << "Break"; break;
      case DxsoOpcode::BreakC: os << "BreakC"; break;
      case DxsoOpcode::Mova: os << "Mova"; break;
      case DxsoOpcode::DefB: os << "DefB"; break;
      case DxsoOpcode::DefI: os << "DefI"; break;

      case DxsoOpcode::TexCoord: os << "TexCoord"; break;
      case DxsoOpcode::TexKill: os << "TexKill"; break;
      case DxsoOpcode::Tex: os << "Tex"; break;
      case DxsoOpcode::TexBem: os << "TexBem"; break;
      case DxsoOpcode::TexBemL: os << "TexBemL"; break;
      case DxsoOpcode::TexReg2Ar: os << "TexReg2Ar"; break;
      case DxsoOpcode::TexReg2Gb: os << "TexReg2Gb"; break;
      case DxsoOpcode::TexM3x2Pad: os << "TexM3x2Pad"; break;
      case DxsoOpcode::TexM3x2Tex: os << "TexM3x2Tex"; break;
      case DxsoOpcode::TexM3x3Pad: os << "TexM3x3Pad"; break;
      case DxsoOpcode::TexM3x3Tex: os << "TexM3x3Tex"; break;
      case DxsoOpcode::Reserved0: os << "Reserved0"; break;
      case DxsoOpcode::TexM3x3Spec: os << "TexM3x3Spec"; break;
      case DxsoOpcode::TexM3x3VSpec: os << "TexM3x3VSpec"; break;
      case DxsoOpcode::ExpP: os << "ExpP"; break;
      case DxsoOpcode::LogP: os << "LogP"; break;
      case DxsoOpcode::Cnd: os << "Cnd"; break;
      case DxsoOpcode::Def: os << "Def"; break;
      case DxsoOpcode::TexReg2Rgb: os << "TexReg2Rgb"; break;
      case DxsoOpcode::TexDp3Tex: os << "TexDp3Tex"; break;
      case DxsoOpcode::TexM3x2Depth: os << "TexM3x2Depth"; break;
      case DxsoOpcode::TexDp3: os << "TexDp3"; break;
      case DxsoOpcode::TexM3x3: os << "TexM3x3"; break;
      case DxsoOpcode::TexDepth: os << "TexDepth"; break;
      case DxsoOpcode::Cmp: os << "Cmp"; break;
      case DxsoOpcode::Bem: os << "Bem"; break;
      case DxsoOpcode::Dp2Add: os << "Dp2Add"; break;
      case DxsoOpcode::DsX: os << "DsX"; break;
      case DxsoOpcode::DsY: os << "DsY"; break;
      case DxsoOpcode::TexLdd: os << "TexLdd"; break;
      case DxsoOpcode::SetP: os << "SetP"; break;
      case DxsoOpcode::TexLdl: os << "TexLdl"; break;
      case DxsoOpcode::BreakP: os << "BreakP"; break;

      case DxsoOpcode::Phase: os << "Phase"; break;
      case DxsoOpcode::Comment: os << "Comment"; break;
      case DxsoOpcode::End: os << "End"; break;
      default:
        os << "Invalid Opcode (" << static_cast<uint32_t>(opcode) << ")"; break;
    }

    return os;
  }

}