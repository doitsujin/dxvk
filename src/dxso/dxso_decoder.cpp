#include "dxso_decoder.h"

#include "dxso_tables.h"

namespace dxvk {

  DxsoShaderInstruction::DxsoShaderInstruction() {}

  DxsoShaderInstruction::DxsoShaderInstruction(const DxsoDecodeContext& context, DxsoCodeIter& iter) {
    m_token             = iter.read();
    m_instructionLength = updateInstructionLength(context);
  }

  uint32_t DxsoShaderInstruction::updateInstructionLength(const DxsoDecodeContext& context) {
    uint32_t length  = 0;
    const auto& info = context.getProgramInfo();

    // Comment ops have their own system for getting length.
    if (opcode() == DxsoOpcode::Comment)
      return (m_token & 0x7fff000) >> 16;

    // SM2.0 and above has the length of the op in instruction count baked into it.
    // SM1.4 and below have fixed lengths and run off expectation.
    // Phase and End do not respect the following rules. :shrug:
    if (opcode() != DxsoOpcode::Phase
     && opcode() != DxsoOpcode::End) {
      if (info.majorVersion() >= 2)
        length = (m_token & 0x0f000000) >> 24;
      else
        length = DxsoGetDefaultOpcodeLength(opcode());
    }

    // We've already logged this...
    if (length == InvalidOpcodeLength)
      return 0;

    // SM 1.4 has an extra param on Tex and TexCoord
    // As stated before, it also doesn't have the length of the op baked into the opcode
    if (info.majorVersion() == 1
     && info.minorVersion() == 4) {
      switch (opcode()) {
        case DxsoOpcode::TexCoord:
        case DxsoOpcode::Tex: length += 1;
        default: break;
      }
    }

    return length;
  }

  bool DxsoSemantic::operator== (const DxsoSemantic& b) const {
    return usage == b.usage && usageIndex == b.usageIndex;
  }

  bool DxsoSemantic::operator!= (const DxsoSemantic& b) const {
    return usage != b.usage || usageIndex != b.usageIndex;
  }

  size_t DxsoSemanticHash::operator () (const DxsoSemantic& key) const {
    std::hash<DxsoUsage> ehash;
    std::hash<uint32_t>  uhash;

    DxvkHashState state;
    state.add(ehash(key.usage));
    state.add(uhash(key.usageIndex));

    return state;
  }

  bool DxsoSemanticEq::operator () (const DxsoSemantic& a, const DxsoSemantic& b) const {
    return a == b;
  }

  DxsoRegister::DxsoRegister()
    : m_type{ DxsoInstructionArgumentType::Source }, m_token{ 0 }, m_relativeToken{ 0 } {}

  DxsoRegister::DxsoRegister(DxsoInstructionArgumentType type, uint32_t token, uint32_t relativeToken) 
    : m_type{ type }, m_token{ token }, m_relativeToken{ relativeToken } {}

  DxsoRegister::DxsoRegister(DxsoInstructionArgumentType type, const DxsoDecodeContext& context, DxsoCodeIter& iter)
    : m_type{ type } {
    m_token         = iter.read();
    m_relativeToken = 0;

    if (this->isRelative()
     && this->relativeAddressingUsesToken(context)) {
      m_relativeToken = iter.read();
      m_hasRelativeToken = true;
    }
  }

  bool DxsoRegister::relativeAddressingUsesToken(const DxsoDecodeContext& context) const {
    auto& info = context.getProgramInfo();

    return (info.majorVersion() >= 2 && m_type == DxsoInstructionArgumentType::Source)
        || (info.majorVersion() >= 3 && m_type == DxsoInstructionArgumentType::Destination);
  }


  void DxsoDecodeContext::decodeDeclaration(DxsoCodeIter& iter) {
    uint32_t dclToken = iter.read();

    m_ctx.dcl.textureType          = static_cast<DxsoTextureType>((dclToken & 0x78000000) >> 27);
    m_ctx.dcl.semantic.usage       = static_cast<DxsoUsage>(dclToken & 0x0000000f);
    m_ctx.dcl.semantic.usageIndex  = (dclToken & 0x000f0000) >> 16;
  }

  void DxsoDecodeContext::decodeDefinition(DxsoOpcode opcode, DxsoCodeIter& iter) {
    const uint32_t instructionLength = std::min(m_ctx.instruction.instructionLength() - 1, 4u);

    std::memset(m_ctx.def.data(), 0, sizeof(m_ctx.def));

    for (uint32_t i = 0; i < instructionLength; i++)
      m_ctx.def[i] = iter.read();
  }

  void DxsoDecodeContext::decodeDestinationRegister(DxsoCodeIter& iter) {
    m_ctx.dst = DxsoRegister(DxsoInstructionArgumentType::Destination, *this, iter);
  }

  void DxsoDecodeContext::decodeSourceRegister(uint32_t i, DxsoCodeIter& iter) {
    if (i >= m_ctx.src.size())
      throw DxvkError("DxsoDecodeContext::decodeSourceRegister: source register out of range.");

    m_ctx.src[i] = DxsoRegister(DxsoInstructionArgumentType::Source, *this, iter);
  }

  void DxsoDecodeContext::decodeInstruction(DxsoCodeIter& iter) {
    m_ctx.instruction = DxsoShaderInstruction(*this, iter);

    const uint32_t instructionLength = m_ctx.instruction.instructionLength();
    const DxsoOpcode opcode          = m_ctx.instruction.opcode();

    if (opcode == DxsoOpcode::If
     || opcode == DxsoOpcode::Ifc
     || opcode == DxsoOpcode::Rep
     || opcode == DxsoOpcode::Loop
     || opcode == DxsoOpcode::BreakC
     || opcode == DxsoOpcode::BreakP) {
      uint32_t sourceIdx = 0;
      for (uint32_t i = 0; i < instructionLength; i++) {
        this->decodeSourceRegister(i, iter);
        if (m_ctx.src[sourceIdx].advanceExtra(*this))
          i++;

        sourceIdx++;
      }
    }
    else if (opcode == DxsoOpcode::Dcl) {
      this->decodeDeclaration(iter);
      this->decodeDestinationRegister(iter);
    }
    else if (opcode == DxsoOpcode::Def
          || opcode == DxsoOpcode::DefI
          || opcode == DxsoOpcode::DefB) {
      this->decodeDestinationRegister(iter);
      this->decodeDefinition(opcode, iter);
    }
    else if (opcode == DxsoOpcode::Comment) {
      // TODO: handle CTAB
      for (uint32_t i = 0; i  < instructionLength; i++)
        iter.read();
    }
    else {
      uint32_t sourceIdx = 0;
      for (uint32_t i = 0; i < instructionLength; i++) {
        if (i == 0) {
          this->decodeDestinationRegister(iter);
          if (m_ctx.dst.advanceExtra(*this))
            i++;
        }
        else {
          this->decodeSourceRegister(sourceIdx, iter);
          if (m_ctx.src[sourceIdx].advanceExtra(*this))
            i++;

          sourceIdx++;
        }
      }
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