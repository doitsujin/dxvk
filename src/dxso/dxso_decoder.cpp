#include "dxso_decoder.h"

#include "dxso_tables.h"

namespace dxvk {

  bool DxsoSemantic::operator== (const DxsoSemantic& b) const {
    return usage == b.usage && usageIndex == b.usageIndex;
  }

  bool DxsoSemantic::operator!= (const DxsoSemantic& b) const {
    return usage != b.usage || usageIndex != b.usageIndex;
  }
  
  uint32_t DxsoDecodeContext::decodeInstructionLength(uint32_t token) {
    auto opcode = m_ctx.instruction.opcode;

    uint32_t length  = 0;
    const auto& info = this->getProgramInfo();

    // Comment ops have their own system for getting length.
    if (opcode == DxsoOpcode::Comment)
      return (token & 0x7fff0000) >> 16;

    if (opcode == DxsoOpcode::End)
      return 0;

    // SM2.0 and above has the length of the op in instruction count baked into it.
    // SM1.4 and below have fixed lengths and run off expectation.
    // Phase does not respect the following rules. :shrug:
    if (opcode != DxsoOpcode::Phase) {
      if (info.majorVersion() >= 2)
        length = (token & 0x0f000000) >> 24;
      else
        length = DxsoGetDefaultOpcodeLength(opcode);
    }

    // We've already logged this...
    if (length == InvalidOpcodeLength)
      return 0;

    // SM 1.4 has an extra param on Tex and TexCoord
    // As stated before, it also doesn't have the length of the op baked into the opcode
    if (info.majorVersion() == 1
     && info.minorVersion() == 4) {
      switch (opcode) {
        case DxsoOpcode::TexCoord:
        case DxsoOpcode::Tex: length += 1;
        default: break;
      }
    }

    return length;
  }

  bool DxsoDecodeContext::relativeAddressingUsesToken(
          DxsoInstructionArgumentType type) {
    auto& info = this->getProgramInfo();

    return (info.majorVersion() >= 2 && type == DxsoInstructionArgumentType::Source)
        || (info.majorVersion() >= 3 && type == DxsoInstructionArgumentType::Destination);
  }

  void DxsoDecodeContext::decodeDeclaration(DxsoCodeIter& iter) {
    uint32_t dclToken = iter.read();

    m_ctx.dcl.textureType          = static_cast<DxsoTextureType>((dclToken & 0x78000000) >> 27);
    m_ctx.dcl.semantic.usage       = static_cast<DxsoUsage>(dclToken & 0x0000000f);
    m_ctx.dcl.semantic.usageIndex  = (dclToken & 0x000f0000) >> 16;
  }

  void DxsoDecodeContext::decodeDefinition(DxsoOpcode opcode, DxsoCodeIter& iter) {
    const uint32_t instructionLength = std::min(m_ctx.instruction.tokenLength - 1, 4u);

    for (uint32_t i = 0; i < instructionLength; i++)
      m_ctx.def.uint32[i] = iter.read();
  }

  void DxsoDecodeContext::decodeBaseRegister(
            DxsoBaseRegister& reg,
            uint32_t          token) {
    reg.id.type = static_cast<DxsoRegisterType>(
        ((token & 0x00001800) >> 8)
      | ((token & 0x70000000) >> 28));

    reg.id.num = token & 0x000007ff;
  }

  void DxsoDecodeContext::decodeGenericRegister(
            DxsoRegister& reg,
            uint32_t      token) {
    this->decodeBaseRegister(reg, token);

    reg.hasRelative = (token & (1 << 13)) == 8192;
    reg.relative.id = DxsoRegisterId {
      DxsoRegisterType::Addr, 0 };
    reg.relative.swizzle = IdentitySwizzle;

    reg.centroid         = token & (4 << 20);
    reg.partialPrecision = token & (2 << 20);
  }

   void DxsoDecodeContext::decodeRelativeRegister(
            DxsoBaseRegister& reg,
            uint32_t          token) {
    this->decodeBaseRegister(reg, token);

    reg.swizzle = DxsoRegSwizzle(
      uint8_t((token & 0x00ff0000) >> 16));
  }

  bool DxsoDecodeContext::decodeDestinationRegister(DxsoCodeIter& iter) {
    uint32_t token = iter.read();

    this->decodeGenericRegister(m_ctx.dst, token);

    m_ctx.dst.mask = DxsoRegMask(
      uint8_t((token & 0x000f0000) >> 16));

    m_ctx.dst.saturate = (token & (1 << 20)) != 0;

    m_ctx.dst.shift    = (token & 0x0f000000) >> 24;
    m_ctx.dst.shift    = (m_ctx.dst.shift & 0x7) - (m_ctx.dst.shift & 0x8);

    const bool extraToken =
      relativeAddressingUsesToken(DxsoInstructionArgumentType::Destination);

    if (m_ctx.dst.hasRelative && extraToken) {
      this->decodeRelativeRegister(m_ctx.dst.relative, iter.read());
      return true;
    }

    return false;
  }

  bool DxsoDecodeContext::decodeSourceRegister(uint32_t i, DxsoCodeIter& iter) {
    if (i >= m_ctx.src.size())
      throw DxvkError("DxsoDecodeContext::decodeSourceRegister: source register out of range.");

    uint32_t token = iter.read();

    this->decodeGenericRegister(m_ctx.src[i], token);

    m_ctx.src[i].swizzle = DxsoRegSwizzle(
      uint8_t((token & 0x00ff0000) >> 16));

    m_ctx.src[i].modifier = static_cast<DxsoRegModifier>(
      (token & 0x0f000000) >> 24);

    const bool extraToken =
      relativeAddressingUsesToken(DxsoInstructionArgumentType::Source);

    if (m_ctx.src[i].hasRelative && extraToken) {
      this->decodeRelativeRegister(m_ctx.src[i].relative, iter.read());
      return true;
    }

    return false;
  }


  void DxsoDecodeContext::decodePredicateRegister(DxsoCodeIter& iter) {
    uint32_t token = iter.read();

    this->decodeGenericRegister(m_ctx.pred, token);

    m_ctx.pred.swizzle = DxsoRegSwizzle(
      uint8_t((token & 0x00ff0000) >> 16));

    m_ctx.pred.modifier = static_cast<DxsoRegModifier>(
      (token & 0x0f000000) >> 24);
  }


  bool DxsoDecodeContext::decodeInstruction(DxsoCodeIter& iter) {
    uint32_t token = iter.read();

    m_ctx.instructionIdx++;

    m_ctx.instruction.opcode = static_cast<DxsoOpcode>(
      token & 0x0000ffff);

    m_ctx.instruction.predicated = token & (1 << 28);

    m_ctx.instruction.coissue    = token & 0x40000000;

    m_ctx.instruction.specificData.uint32 =
      (token & 0x00ff0000) >> 16;

    m_ctx.instruction.tokenLength =
      this->decodeInstructionLength(token);

    uint32_t tokenLength =
      m_ctx.instruction.tokenLength;

    switch (m_ctx.instruction.opcode) {
      case DxsoOpcode::If:
      case DxsoOpcode::Ifc:
      case DxsoOpcode::Rep:
      case DxsoOpcode::Loop:
      case DxsoOpcode::BreakC:
      case DxsoOpcode::BreakP: {
        uint32_t sourceIdx = 0;
        for (uint32_t i = 0; i < tokenLength; i++) {
          if (this->decodeSourceRegister(sourceIdx, iter))
            i++;

          sourceIdx++;
        }
        return true;
      }

      case DxsoOpcode::Dcl:
        this->decodeDeclaration(iter);
        this->decodeDestinationRegister(iter);
        return true;

      case DxsoOpcode::Def:
      case DxsoOpcode::DefI:
      case DxsoOpcode::DefB:
        this->decodeDestinationRegister(iter);
        this->decodeDefinition(
          m_ctx.instruction.opcode, iter);
        return true;

      case DxsoOpcode::Comment:
        iter = iter.skip(tokenLength);
        return true;

      default: {
        uint32_t sourceIdx = 0;
        for (uint32_t i = 0; i < tokenLength; i++) {
          if (i == 0) {
            if (this->decodeDestinationRegister(iter))
              i++;
          }
          else if (i == 1 && m_ctx.instruction.predicated) {
            // Relative addressing makes no sense
            // for predicate registers.
            this->decodePredicateRegister(iter);
          }
          else {
            if (this->decodeSourceRegister(sourceIdx, iter))
              i++;

            sourceIdx++;
          }
        }
        return true;
      }

      case DxsoOpcode::End:
        return false;
    }
  }

  std::ostream& operator << (std::ostream& os, DxsoUsage usage) {
    switch (usage) {
      case DxsoUsage::Position:     os << "Position"; break;
      case DxsoUsage::BlendWeight:  os << "BlendWeight"; break;
      case DxsoUsage::BlendIndices: os << "BlendIndices"; break;
      case DxsoUsage::Normal:       os << "Normal"; break;
      case DxsoUsage::PointSize:    os << "PointSize"; break;
      case DxsoUsage::Texcoord:     os << "Texcoord"; break;
      case DxsoUsage::Tangent:      os << "Tangent"; break;
      case DxsoUsage::Binormal:     os << "Binormal"; break;
      case DxsoUsage::TessFactor:   os << "TessFactor"; break;
      case DxsoUsage::PositionT:    os << "PositionT"; break;
      case DxsoUsage::Color:        os << "Color"; break;
      case DxsoUsage::Fog:          os << "Fog"; break;
      case DxsoUsage::Depth:        os << "Depth"; break;
      case DxsoUsage::Sample:       os << "Sample"; break;
      default:
        os << "Invalid Format (" << static_cast<uint32_t>(usage) << ")"; break;
    }

    return os;
  }

}