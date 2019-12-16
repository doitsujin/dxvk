#include "dxso_tables.h"

namespace dxvk {

  uint32_t DxsoGetDefaultOpcodeLength(DxsoOpcode opcode) {

    switch (opcode) {
      case DxsoOpcode::Nop: return 0;
      case DxsoOpcode::Mov: return 2;
      case DxsoOpcode::Add: return 3;
      case DxsoOpcode::Sub: return 3;
      case DxsoOpcode::Mad: return 4;
      case DxsoOpcode::Mul: return 3;
      case DxsoOpcode::Rcp: return 2;
      case DxsoOpcode::Rsq: return 2;
      case DxsoOpcode::Dp3: return 3;
      case DxsoOpcode::Dp4: return 3;
      case DxsoOpcode::Min: return 3;
      case DxsoOpcode::Max: return 3;
      case DxsoOpcode::Slt: return 3;
      case DxsoOpcode::Sge: return 3;
      case DxsoOpcode::Exp: return 2;
      case DxsoOpcode::Log: return 2;
      case DxsoOpcode::Lit: return 2;
      case DxsoOpcode::Dst: return 3;
      case DxsoOpcode::Lrp: return 4;
      case DxsoOpcode::Frc: return 2;
      case DxsoOpcode::M4x4: return 3;
      case DxsoOpcode::M4x3: return 3;
      case DxsoOpcode::M3x4: return 3;
      case DxsoOpcode::M3x3: return 3;
      case DxsoOpcode::M3x2: return 3;
      case DxsoOpcode::Call: return 1;
      case DxsoOpcode::CallNz: return 2;
      case DxsoOpcode::Loop: return 2;
      case DxsoOpcode::Ret: return 0;
      case DxsoOpcode::EndLoop: return 0;
      case DxsoOpcode::Label: return 1;
      case DxsoOpcode::Dcl: return 2;
      case DxsoOpcode::Pow: return 3;
      case DxsoOpcode::Crs: return 3;
      case DxsoOpcode::Sgn: return 4;
      case DxsoOpcode::Abs: return 2;
      case DxsoOpcode::Nrm: return 2;
      case DxsoOpcode::SinCos: return 4;
      case DxsoOpcode::Rep: return 1;
      case DxsoOpcode::EndRep: return 0;
      case DxsoOpcode::If: return 1;
      case DxsoOpcode::Ifc: return 2;
      case DxsoOpcode::Else: return 0;
      case DxsoOpcode::EndIf: return 0;
      case DxsoOpcode::Break: return 0;
      case DxsoOpcode::BreakC: return 2;
      case DxsoOpcode::Mova: return 2;
      case DxsoOpcode::DefB: return 2;
      case DxsoOpcode::DefI: return 5;
      case DxsoOpcode::TexCoord: return 1;
      case DxsoOpcode::TexKill: return 1;
      case DxsoOpcode::Tex: return 1;
      case DxsoOpcode::TexBem: return 2;
      case DxsoOpcode::TexBemL: return 2;
      case DxsoOpcode::TexReg2Ar: return 2;
      case DxsoOpcode::TexReg2Gb: return 2;
      case DxsoOpcode::TexM3x2Pad: return 2;
      case DxsoOpcode::TexM3x2Tex: return 2;
      case DxsoOpcode::TexM3x3Pad: return 2;
      case DxsoOpcode::TexM3x3Tex: return 2;
      case DxsoOpcode::TexM3x3Spec: return 3;
      case DxsoOpcode::TexM3x3VSpec: return 2;
      case DxsoOpcode::ExpP: return 2;
      case DxsoOpcode::LogP: return 2;
      case DxsoOpcode::Cnd: return 4;
      case DxsoOpcode::Def: return 5;
      case DxsoOpcode::TexReg2Rgb: return 2;
      case DxsoOpcode::TexDp3Tex: return 2;
      case DxsoOpcode::TexM3x2Depth: return 2;
      case DxsoOpcode::TexDp3: return 2;
      case DxsoOpcode::TexM3x3: return 2;
      case DxsoOpcode::TexDepth: return 1;
      case DxsoOpcode::Cmp: return 4;
      case DxsoOpcode::Bem: return 3;
      case DxsoOpcode::Dp2Add: return 4;
      case DxsoOpcode::DsX: return 2;
      case DxsoOpcode::DsY: return 2;
      case DxsoOpcode::TexLdd: return 5;
      case DxsoOpcode::SetP: return 3;
      case DxsoOpcode::TexLdl: return 3;
      case DxsoOpcode::BreakP: return 2;
      default: Logger::warn("DxsoGetDefaultOpcodeLength: unknown opcode to get default length for."); return UINT32_MAX;
    }
  }

}