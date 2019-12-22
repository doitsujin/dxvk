#pragma once

#include "dxso_common.h"
#include "dxso_enums.h"
#include "dxso_code.h"

namespace dxvk {

  constexpr size_t DxsoMaxTempRegs      = 32;
  constexpr size_t DxsoMaxTextureRegs   = 10;
  constexpr size_t DxsoMaxInterfaceRegs = 16;
  constexpr size_t DxsoMaxOperandCount  = 8;

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
    Bias    = 2,  // r - 0.5
    BiasNeg = 3,  // -(r - 0.5)
    Sign    = 4,  // fma(r, 2.0f, -1.0f)
    SignNeg = 5,  // -fma(r, 2.0f, -1.0f)
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

  enum class DxsoTexLdMode : uint32_t {
    Regular      = 0,
    Project      = 1,
    Bias         = 2
  };

  union DxsoOpcodeSpecificData {
    DxsoComparison comparison;
    DxsoTexLdMode  texld;

    uint32_t       uint32;
  };

  struct DxsoShaderInstruction {
    DxsoOpcode             opcode;
    bool                   predicated;
    bool                   coissue;
    DxsoOpcodeSpecificData specificData;

    uint32_t               tokenLength;
  };

  struct DxsoRegisterId {
    DxsoRegisterType type;
    uint32_t         num;

    bool operator == (const DxsoRegisterId& other) const { return type == other.type && num == other.num; }
    bool operator != (const DxsoRegisterId& other) const { return type != other.type || num != other.num; }
  };

  class DxsoRegMask {

  public:

    DxsoRegMask(uint8_t mask)
      : m_mask(mask) { }

    DxsoRegMask(bool x, bool y, bool z, bool w)
    : m_mask((x ? 0x1 : 0) | (y ? 0x2 : 0)
           | (z ? 0x4 : 0) | (w ? 0x8 : 0)) { }

    bool operator [] (uint32_t id) const {
      return ((m_mask & (1u << id)) != 0);
    }

    uint32_t popCount() const {
      const uint8_t n[16] = { 0, 1, 1, 2, 1, 2, 2, 3,
                              1, 2, 2, 3, 2, 3, 3, 4 };
      return n[m_mask & 0xF];
    }
    
    uint32_t firstSet() const {
      const uint8_t n[16] = { 4, 0, 1, 0, 2, 0, 1, 0,
                              3, 0, 1, 0, 2, 0, 1, 0 };
      return n[m_mask & 0xF];
    }
    
    uint32_t minComponents() const {
      const uint8_t n[16] = { 0, 1, 2, 2, 3, 3, 3, 3,
                              4, 4, 4, 4, 4, 4, 4, 4 };
      return n[m_mask & 0xF];
    }

    bool operator == (const DxsoRegMask& other) const { return m_mask == other.m_mask; }
    bool operator != (const DxsoRegMask& other) const { return m_mask != other.m_mask; }

  private:

    uint8_t m_mask;

  };

  const DxsoRegMask IdentityWriteMask = DxsoRegMask(true, true, true, true);

  class DxsoRegSwizzle {

  public:

    DxsoRegSwizzle(uint8_t mask)
      : m_mask(mask) { }

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

  const DxsoRegSwizzle IdentitySwizzle{ 0, 1, 2, 3 };

  struct DxsoBaseRegister {
    DxsoRegisterId  id               = { DxsoRegisterType::Temp, 0 };
    bool            centroid         = false;
    bool            partialPrecision = false;
    bool            saturate         = false;
    DxsoRegModifier modifier         = DxsoRegModifier::None;
    DxsoRegMask     mask             = IdentityWriteMask;
    DxsoRegSwizzle  swizzle          = IdentitySwizzle;
    int8_t          shift            = 0;
  };

  struct DxsoRegister : public DxsoBaseRegister {
    bool hasRelative = false;
    DxsoBaseRegister relative;
  };

  struct DxsoSemantic {
    DxsoUsage       usage;
    uint32_t        usageIndex;

    bool operator== (const DxsoSemantic& b) const;
    bool operator!= (const DxsoSemantic& b) const;
  };

  struct DxsoDeclaration {
    DxsoSemantic    semantic;

    DxsoTextureType textureType;
  };

  union DxsoDefinition {
    float    float32[4];
    int32_t  int32[4];

    // Not a type we actually use in compiler, but used for decoding.
    uint32_t uint32[4];
  };

  struct DxsoInstructionContext {
    uint32_t                    instructionIdx;

    DxsoShaderInstruction       instruction;

    DxsoRegister                pred;

    DxsoRegister                dst;
    std::array<
      DxsoRegister,
      DxsoMaxOperandCount>      src;

    DxsoDefinition              def;

    DxsoDeclaration             dcl;
  };

  class DxsoDecodeContext {

  public:

    DxsoDecodeContext(const DxsoProgramInfo& programInfo)
      : m_programInfo( programInfo ) {
      m_ctx.instructionIdx = 0;
    }

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
    bool decodeInstruction(DxsoCodeIter& iter);

  private:

    uint32_t decodeInstructionLength(uint32_t token);

    void decodeBaseRegister(
            DxsoBaseRegister& reg,
            uint32_t          token);
    void decodeGenericRegister(
            DxsoRegister& reg,
            uint32_t      token);
    void decodeRelativeRegister(
            DxsoBaseRegister& reg,
            uint32_t          token);

    // Returns whether an extra token was read.
    bool decodeDestinationRegister(DxsoCodeIter& iter);
    bool decodeSourceRegister(uint32_t i, DxsoCodeIter& iter);
    void decodePredicateRegister(DxsoCodeIter& iter);

    void decodeDeclaration(DxsoCodeIter& iter);
    void decodeDefinition(DxsoOpcode opcode, DxsoCodeIter& iter);

    bool relativeAddressingUsesToken(DxsoInstructionArgumentType type);

    const DxsoProgramInfo&      m_programInfo;

    DxsoInstructionContext      m_ctx;

  };

  std::ostream& operator << (std::ostream& os, DxsoUsage usage);

}