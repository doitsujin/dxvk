#pragma once

#include "dxso_common.h"
#include "dxso_enums.h"
#include "dxso_code.h"

namespace dxvk {

  constexpr uint32_t DxsoRegModifierShift = 24;

  class DxsoDecodeContext;

  /**
   * \brief Source operand modifiers
   * 
   * These are applied after loading
   * an operand register.
   */
  enum class DxsoRegModifier : uint32_t {
    None    = 0,  // r
    Neg     = 1,  // -r
    Bias    = 2,  // Exponent Bias (treating as r)
    BiasNeg = 3,  // Exponent Bias (treating as -r)
    Sign    = 4,  // Sign (treating as r)
    SignNeg = 5,  // Sign (treating as -r)
    Comp    = 6,  // 1 - r
    X2      = 7,  // r * 2
    X2Neg   = 8,  // -r * 2
    Dz      = 9,  // r / r.z
    Dw      = 10, // r / r.w
    Abs     = 11, // abs(r)
    AbsNeg  = 12, // -abs(r)
    Not     = 13, // !r
  };

  enum class DxsoInstructionArgumentType : uint16_t {
    Source,
    Destination
  };

  enum class DxsoComparison : uint32_t {
                             // < = >
    Never        = 0,        // 0 0 0
    GreaterThan  = 1,        // 0 0 1
    Equal        = 2,        // 0 1 0
    GreaterEqual = 3,        // 0 1 1
    LessThan     = 4,        // 1 0 0
    NotEqual     = 5,        // 1 0 1
    LessEqual    = 6,        // 1 1 0
    Always       = 7         // 1 1 1
  };

  class DxsoShaderInstruction {

  public:

    DxsoShaderInstruction();
    DxsoShaderInstruction(const DxsoDecodeContext& context, DxsoCodeSlice& slice);

    DxsoOpcode opcode() const {
      return static_cast<DxsoOpcode>(m_token & 0x0000ffff);
    }

    uint32_t opcodeSpecificData() const {
      return (m_token & 0x00ff0000) >> 16;
    }

    uint32_t instructionLength() const {
      return m_instructionLength;
    }

    DxsoComparison comparison() const {
      return static_cast<DxsoComparison>(this->opcodeSpecificData());
    }

  private:

    uint32_t updateInstructionLength(const DxsoDecodeContext& context);

    uint32_t m_token;
    uint32_t m_instructionLength;

  };

  class DxsoRegisterId {

  public:

    DxsoRegisterId() {}

    DxsoRegisterId(DxsoRegisterType type, uint32_t num)
      : m_type{ type }, m_num{ num } {}

    DxsoRegisterType type() const {
      return m_type;
    }

    uint32_t num() const {
      return m_num;
    }

    bool constant() const {
      return m_type == DxsoRegisterType::Const
          || m_type == DxsoRegisterType::ConstInt
          || m_type == DxsoRegisterType::ConstBool
          || m_type == DxsoRegisterType::Const2
          || m_type == DxsoRegisterType::Const3
          || m_type == DxsoRegisterType::Const4;
    }

    bool operator == (const DxsoRegisterId& other) const { return m_type == other.m_type
                                                               && m_num  == other.m_num; }
    bool operator != (const DxsoRegisterId& other) const { return m_type != other.m_type
                                                               || m_num  != other.m_num; }

  private:

    DxsoRegisterType m_type;
    uint32_t         m_num;

  };

  class DxsoRegMask {

  public:

    DxsoRegMask(uint32_t token)
      : m_mask{ static_cast<uint8_t>( (token & 0x000f0000) >> 16 ) } {}

    DxsoRegMask(bool x, bool y, bool z, bool w)
    : m_mask((x ? 0x1 : 0) | (y ? 0x2 : 0)
           | (z ? 0x4 : 0) | (w ? 0x8 : 0)) { }

    bool operator [] (uint32_t id) const {
      return ((m_mask & (1u << id)) != 0);
    }

    bool operator == (const DxsoRegMask& other) const { return m_mask == other.m_mask; }
    bool operator != (const DxsoRegMask& other) const { return m_mask != other.m_mask; }

  private:

    uint8_t m_mask;

  };

  const DxsoRegMask IdentityWriteMask = DxsoRegMask(true, true, true, true);

  class DxsoRegSwizzle {

  public:

    DxsoRegSwizzle(uint32_t token)
      : m_mask{ static_cast<uint8_t>( (token & 0x00ff0000) >> 16 ) } {}

    DxsoRegSwizzle(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
      : m_mask((x << 0) | (y << 2) | (z << 4) | (w << 6)) {}

    uint32_t operator [] (uint32_t id) const {
      return (m_mask >> (id + id)) & 0x3;
    }

    bool operator == (const DxsoRegSwizzle& other) const { return m_mask == other.m_mask; }
    bool operator != (const DxsoRegSwizzle& other) const { return m_mask != other.m_mask; }

  private:

    uint8_t m_mask;

  };

  const DxsoRegSwizzle IdentitySwizzle{ 1, 2, 3, 4 };

  class DxsoRegister {

  public:

    DxsoRegister();
    DxsoRegister(DxsoInstructionArgumentType type, uint32_t token, uint32_t relativeToken);
    DxsoRegister(DxsoInstructionArgumentType type, const DxsoDecodeContext& context, DxsoCodeSlice& slice);

    DxsoRegisterId registerId() const {
      return DxsoRegisterId{ registerType(), registerNumber() };
    }

    bool isRelative() const {
      return (m_token & (1 << 13)) == 8192 ? 1 : 0;
    }

    DxsoRegister relativeRegister() const {
      return DxsoRegister(DxsoInstructionArgumentType::Source, m_relativeToken, m_relativeToken);
    }

    bool centroid() const {
      return m_token & (4 << 20);
    }

    bool partialPrecision() const {
      return m_token & (2 << 20);
    }

    bool saturate() const {
      if (m_type == DxsoInstructionArgumentType::Source)
        throw DxvkError("Attempted to read the saturate of a Src register.");

      return (m_token & (1 << 20)) != 0;
    }

    DxsoRegModifier modifier() const {
      if (m_type == DxsoInstructionArgumentType::Destination)
        throw DxvkError("Attempted to read the modifier of a Dst register.");

      return static_cast<DxsoRegModifier>(
        (m_token & 0x0f000000) >> 24);
    }

    DxsoRegMask writeMask() const {
      if (m_type == DxsoInstructionArgumentType::Source)
        throw DxvkError("Attempted to read the write mask of a Src register.");

      return DxsoRegMask{ m_token };
    }

    DxsoRegSwizzle swizzle() const {
      if (m_type == DxsoInstructionArgumentType::Destination)
        throw DxvkError("Attempted to read the swizzle of a Dst register.");

      return DxsoRegSwizzle{ m_token };
    }

    bool advanceExtra(const DxsoDecodeContext& context) {
      return isRelative() && relativeAddressingUsesToken(context);
    }

    bool hasRelativeToken() const {
      return m_hasRelativeToken;
    }

  private:

    DxsoRegisterType registerType() const {
      return static_cast<DxsoRegisterType>(
        ((m_token & 0x00001800) >> 8)
      | ((m_token & 0x70000000) >> 28));
    }

    uint32_t registerNumber() const {
      return m_token & 0x000007ff;
    }

    bool relativeAddressingUsesToken(const DxsoDecodeContext& context) const;

    DxsoInstructionArgumentType m_type;

    uint32_t                    m_token;
    uint32_t                    m_relativeToken;
    bool                        m_hasRelativeToken;

  };

  struct DxsoSemantic {
    DxsoUsage       usage;
    uint32_t        usageIndex;

    bool operator== (const DxsoSemantic& b) const;
    bool operator!= (const DxsoSemantic& b) const;
  };

  struct DxsoSemanticHash {
    size_t operator () (const DxsoSemantic& key) const;
  };

  struct DxsoSemanticEq {
    bool operator () (const DxsoSemantic& a, const DxsoSemantic& b) const;
  };

  // This struct doesn't work off the single tokens
  // because we want to make a list of declarations
  // and that would include the implicit declarations in
  // lower shader model versions.
  struct DxsoDeclaration {
    DxsoRegisterId  id;

    DxsoSemantic    semantic;

    DxsoTextureType textureType;
  };

  struct DxsoInstructionContext {
    DxsoShaderInstruction       instruction;

    DxsoRegister                dst;
    std::array<DxsoRegister, 4> src;

    std::array<uint32_t, 4>     def;

    DxsoDeclaration             dcl;
  };

  class DxsoDecodeContext {

  public:

    DxsoDecodeContext(const DxsoProgramInfo& programInfo)
      : m_programInfo{ programInfo } {}

    /**
     * \brief Retrieves current instruction context
     *
     * This is only valid after a call to \ref decode.
     * \returns Reference to last decoded instruction & its context
     */
    const DxsoInstructionContext& getInstructionContext() const {
      return m_ctx;
    }

    const DxsoProgramInfo& getProgramInfo() const {
      return m_programInfo;
    }

    /**
     * \brief Decodes an instruction
     *
     * This also advances the given code slice by the
     * number of dwords consumed by the instruction.
     * \param [in] code Code slice
     */
    void decodeInstruction(DxsoCodeSlice& code);

  private:

    void decodeDestinationRegister(DxsoCodeSlice& code);
    void decodeSourceRegister(uint32_t i, DxsoCodeSlice& code);
    void decodeDeclaration(DxsoCodeSlice& code);
    void decodeDefinition(DxsoOpcode opcode, DxsoCodeSlice& code);

    const DxsoProgramInfo&      m_programInfo;

    DxsoInstructionContext      m_ctx;

  };

  std::ostream& operator << (std::ostream& os, DxsoUsage usage);

}