#include "dxso_decoder.h"

#include "dxso_tables.h"

namespace dxvk {

  DxsoShaderInstruction::DxsoShaderInstruction() {}

  DxsoShaderInstruction::DxsoShaderInstruction(const DxsoDecodeContext& context, DxsoCodeSlice& slice) {
    m_token             = slice.read();
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

  DxsoRegister::DxsoRegister()
    : m_type{ DxsoInstructionArgumentType::Source }, m_token{ 0 }, m_relativeIndex{ 0 } {}

  DxsoRegister::DxsoRegister(DxsoInstructionArgumentType type, uint32_t token, uint32_t relativeToken) 
    : m_type{ type }, m_token{ token }, m_relativeIndex{ relativeToken } {}

  DxsoRegister::DxsoRegister(DxsoInstructionArgumentType type, const DxsoDecodeContext& context, DxsoCodeSlice& slice)
    : m_type{ type } {
    m_token         = slice.read();
    m_relativeIndex = 0;

    if (this->isRelative()
     && this->relativeAddressingUsesToken(context)) {
      m_relativeIndex = slice.read();
    }
  }

  bool DxsoRegister::relativeAddressingUsesToken(const DxsoDecodeContext& context) const {
    auto& info = context.getProgramInfo();

    return (info.majorVersion() >= 2 && m_type == DxsoInstructionArgumentType::Source)
        || (info.majorVersion() >= 3 && m_type == DxsoInstructionArgumentType::Destination);
  }


  void DxsoDecodeContext::decodeDeclaration(DxsoCodeSlice& code) {
    uint32_t dclToken = code.read();

    m_ctx.dcl.reg         = m_ctx.dst;
    m_ctx.dcl.textureType = static_cast<DxsoTextureType>((dclToken & 0x78000000) >> 27);
    m_ctx.dcl.usage       = static_cast<DxsoUsage>(dclToken & 0x0000000f);
    m_ctx.dcl.usageIndex  = (dclToken & 0x000f0000) >> 16;
  }

  void DxsoDecodeContext::decodeDefinition(DxsoOpcode opcode, DxsoCodeSlice& code) {
    const uint32_t instructionLength = std::min(m_ctx.instruction.instructionLength(), 4u);

    std::memset(m_ctx.def.data(), 0, sizeof(m_ctx.def));

    for (uint32_t i = 0; i < instructionLength; i++)
      m_ctx.def[i] = code.read();
  }

  void DxsoDecodeContext::decodeDestinationRegister(DxsoCodeSlice& code) {
    m_ctx.dst = DxsoRegister(DxsoInstructionArgumentType::Destination, *this, code);
  }

  void DxsoDecodeContext::decodeSourceRegister(uint32_t i, DxsoCodeSlice& code) {
    if (i >= m_ctx.src.size())
      throw DxvkError("DxsoDecodeContext::decodeSourceRegister: source register out of range.");

    m_ctx.src[i] = DxsoRegister(DxsoInstructionArgumentType::Source, *this, code);
  }

  void DxsoDecodeContext::decodeInstruction(DxsoCodeSlice& code) {
    m_ctx.instruction = DxsoShaderInstruction(*this, code);

    const uint32_t instructionLength = m_ctx.instruction.instructionLength();
    const DxsoOpcode opcode          = m_ctx.instruction.opcode();

    if (opcode == DxsoOpcode::If
     || opcode == DxsoOpcode::Ifc
     || opcode == DxsoOpcode::Loop
     || opcode == DxsoOpcode::BreakC
     || opcode == DxsoOpcode::BreakP) {
      for (uint32_t i = 0; i < instructionLength; i++)
        this->decodeSourceRegister(i, code);
    }
    else if (opcode == DxsoOpcode::Dcl) {
      this->decodeDestinationRegister(code);
      this->decodeDeclaration(code);
    }
    else if (opcode == DxsoOpcode::Def
          || opcode == DxsoOpcode::DefI
          || opcode == DxsoOpcode::DefB) {
      this->decodeDestinationRegister(code);
      this->decodeDefinition(opcode, code);
    }
    else {
      for (uint32_t i = 0; i < instructionLength; i++) {
        if (i == 0)
          this->decodeDestinationRegister(code);
        else {
          const uint32_t sourceIndex = i - 1u; // We used the first instruction for the dst, hence the i - 1.
          this->decodeSourceRegister(sourceIndex, code);
        }
      }
    }
  }

}