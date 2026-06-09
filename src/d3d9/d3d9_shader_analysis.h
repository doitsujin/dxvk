#pragma once

#include "d3d9_constant_copy.h"

#include "../util/util_vector.h"
#include "../util/util_small_vector.h"

#include <util/util_byte_stream.h>

#include <sm3/sm3_types.h>
#include <sm3/sm3_parser.h>

#include <vulkan/vulkan.h>

#include <vector>

namespace dxvk {

struct D3D9ImmediateConstantsData {
  small_vector<D3D9ImmediateFloatConstant, 16u> floats;
  small_vector<int16_t, 16u> ints;
};

struct D3D9ImmediateConstantsInfo {
  uint32_t floatCount = 0u;
  uint32_t intCount   = 0u;
  uint32_t boolCount  = 0u;
};

struct D3D9ShaderConstantsInfo {
  bool floatsAccessedDynamically = false;

  uint32_t floatCount = 0u;
  uint32_t intCount   = 0u;
  uint32_t boolCount  = 0u;

  uint32_t boolMask = 0u;
};

using D3D9RenderTargetMask = uint8_t;
using D3D9SamplerMask      = uint32_t;

/**
 * \brief Packed semantic
 *
 * Extremely compact representation of a semantic
 * that can be used for look-up purposes.
 */
struct D3D9PackedSemantic {
  D3D9PackedSemantic() = default;

  explicit D3D9PackedSemantic(uint8_t packed)
  : usage(packed), index(packed >> 4u) { }

  explicit D3D9PackedSemantic(dxbc_spv::sm3::Semantic semantic)
  : usage(uint8_t(semantic.usage))
  , index(uint8_t(semantic.index)) { }

  explicit operator dxbc_spv::sm3::Semantic() const {
    dxbc_spv::sm3::Semantic semantic = {};
    semantic.usage = dxbc_spv::sm3::SemanticUsage(usage);
    semantic.index = index;
    return semantic;
  }

  explicit operator uint8_t() const {
    return uint8_t(usage) | (uint8_t(index) << 4u);
  }

  uint8_t usage : 4u;
  uint8_t index : 4u;
};

/**
 * \brief Input signature
 *
 * Fixed-size structure that stores the semantic that
 * corresponds to any given vertex shader inputs.
 */
class D3D9InputSignature {
  // Abuse last element as the element count so that we
  // can fit the entire structure into two SSE registers
  static constexpr size_t SizeIndex = 31u;
public:

  D3D9InputSignature() {
    // Populate with 'invalid' semantics
    for (size_t i = 0u; i < SizeIndex; i++)
      m_entries[i] = 0xffu;
  }

  /**
   * \brief Number of valid entries in the signature
   * \returns Number of signature entries
   */
  uint32_t size() const {
    return m_entries[SizeIndex];
  }

  /**
   * \brief Retrieves packed representation of a semantic
   *
   * \param [in] index Input register index
   * \returns Semantic bound to the given register
   */
  D3D9PackedSemantic getPacked(uint32_t index) const {
    return D3D9PackedSemantic(m_entries[index]);
  }

  /**
   * \brief Retrieves semantic for a given register
   *
   * \param [in] index Input register index
   * \returns Semantic bound to the given register
   */
  dxbc_spv::sm3::Semantic get(uint32_t index) const {
    return dxbc_spv::sm3::Semantic(getPacked(index));
  }

  /**
   * \brief Adds an input register with a given semantic
   * \param [in] semantic Semantic to add
   */
  void add(dxbc_spv::sm3::Semantic semantic) {
    dxbc_spv_assert(semantic.index < 16u);

    auto index = m_entries[SizeIndex]++;
    m_entries.at(index) = uint8_t(D3D9PackedSemantic(semantic));
  }

  /**
   * \brief Finds input register for a given semantic
   *
   * Returns an out-of-bounds index if no register
   * uses the given semantic.
   * \param [in] semantic Semantic to find
   * \returns Register index, if any, for given semantic
   */
  uint32_t find(dxbc_spv::sm3::Semantic semantic) const {
    auto packed = uint8_t(D3D9PackedSemantic(semantic));

    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    __m128i compareMask = _mm_set1_epi32(uint32_t(packed) * 0x01010101u);

    __m128i eq0 = _mm_cmpeq_epi8(compareMask, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&m_entries[ 0u])));
    __m128i eq1 = _mm_cmpeq_epi8(compareMask, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&m_entries[16u])));

    uint32_t bits0 = _mm_movemask_epi8(eq0);
    uint32_t bits1 = _mm_movemask_epi8(eq1);

    return bit::tzcnt(bits0 | (bits1 << 16u));
    #else
    for (uint32_t i = 0u; i < SizeIndex; i++) {
      if (m_entries[i] == packed)
        return i;
    }

    return m_entries.size();
    #endif
  }

private:

  std::array<uint8_t, SizeIndex + 1u> m_entries = {};

};

class D3D9ShaderAnalysis {

public:

  D3D9ShaderAnalysis() = default;

  D3D9ShaderAnalysis(dxbc_spv::util::ByteReader code, bool isSWVP);

  D3D9ShaderAnalysis(const D3D9ShaderAnalysis& other) = default;
  D3D9ShaderAnalysis(D3D9ShaderAnalysis&& other) = default;

  D3D9ShaderAnalysis& operator = (const D3D9ShaderAnalysis& other) = default;
  D3D9ShaderAnalysis& operator = (D3D9ShaderAnalysis&& other) = default;

  dxbc_spv::sm3::ShaderInfo GetShaderInfo() const {
    return m_shaderInfo;
  }

  size_t GetLength() const {
    return m_length;
  }

  const D3D9ShaderConstantsInfo& GetConstantsInfo() const {
    return m_constants;
  }

  const D3D9ImmediateConstantsInfo& GetImmediateConstants() const {
    return m_immediateConstants;
  }

  D3D9RenderTargetMask GetRenderTargetMask() const {
    return m_usedRTs;
  }

  const D3D9ConstantBufferCopy* GetConstantLayout() const {
    return m_constLayout;
  }

  D3D9SamplerMask GetSamplerMask() const {
    return m_usedSamplers;
  }

  VkImageViewType GetImageViewType(uint32_t index) const {
    return m_imageViewTypes[index];
  }

  bool IsSWVP() const {
    return m_isSWVP;
  }

  const D3D9InputSignature& GetInputSignature() const { return m_inputSignature; }

  uint32_t GetFlatShadingMask() const {
    return m_flatShadingMask;
  }

  explicit operator bool() const {
    return m_length != 0u;
  }

private:

  using ConstantMask = small_vector<uint32_t, 64u>;

  bool RunAnalysis(dxbc_spv::sm3::Parser& parser);

  bool HandleInstruction(
    const dxbc_spv::sm3::Instruction&   op,
          ConstantMask&                 constMaskF,
          ConstantMask&                 constMaskI,
          D3D9ImmediateConstantsData&   shaderDefs);

  bool HandleDef(
    const dxbc_spv::sm3::Instruction&   op,
          D3D9ImmediateConstantsData&   shaderDefs);

  bool HandleTextureSample(const dxbc_spv::sm3::Instruction& op);

  bool HandleDcl(const dxbc_spv::sm3::Instruction& op);

  std::optional<uint32_t> FindLocationInFixedFunctionIO(dxbc_spv::sm3::Semantic semantic) const;

  bool m_isSWVP = false;

  uint32_t m_length = 0u;

  dxbc_spv::sm3::ShaderInfo m_shaderInfo;

  D3D9ShaderConstantsInfo m_constants;
  D3D9ImmediateConstantsInfo m_immediateConstants;
  const D3D9ConstantBufferCopy* m_constLayout;

  D3D9RenderTargetMask m_usedRTs = 0u;

  D3D9SamplerMask m_usedSamplers = 0u;

  std::array<VkImageViewType, 16u> m_imageViewTypes = {};

  uint32_t m_flatShadingMask = 0u;

  D3D9InputSignature m_inputSignature = {};

  static void setBit(ConstantMask& mask, uint32_t bit);

  static void clrBit(ConstantMask& mask, uint32_t bit);

};

}
