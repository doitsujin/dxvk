#include <cmath>

#include "d3d9_constant_set.h"

#include "../util/util_bit.h"

namespace dxvk {

  dxvk::mutex D3D9ConstantBufferCopy::s_mutex;
  std::unordered_set<D3D9ConstantBufferCopy, DxvkHash, DxvkEq> D3D9ConstantBufferCopy::s_layouts;

  D3D9ConstantBufferLayout::D3D9ConstantBufferLayout() { }

  D3D9ConstantBufferLayout::D3D9ConstantBufferLayout(uint32_t maxCount)
  : m_minCount(maxCount), m_maxCount(maxCount) {
    D3D9ConstantRange range = {};
    range.dstIndex = 0u;
    range.srcIndex = 0u;
    range.count = maxCount;

    addRange(range);
  }

  D3D9ConstantBufferLayout::D3D9ConstantBufferLayout(uint32_t useLength, const uint32_t* useMask) {
    // Tightly pack accessed constants
    uint32_t dstIndex = 0u;

    for (uint32_t i = 0u; i < useLength; i++) {
      uint32_t mask = useMask[i];

      while (mask) {
        uint32_t bitIndex = bit::tzcnt(mask);
        uint32_t bitCount = bit::tzcnt(mask + (1u << bitIndex)) - bitIndex;

        D3D9ConstantRange range = {};
        range.dstIndex = dstIndex;
        range.srcIndex = bitIndex + 32u * i;
        range.count = bitCount;

        addRange(range);
        dstIndex += range.count;

        mask &= -2u << (bitIndex + bitCount - 1u);
      }
    }

    m_minCount = dstIndex;
    m_maxCount = dstIndex;
  }

  D3D9ConstantBufferLayout::D3D9ConstantBufferLayout(
          uint32_t            maxCount,
          uint32_t            useLength,
    const uint32_t*           useMask,
          size_t              defCount,
    const D3D9ImmediateFloatConstant* defData)
  : m_minCount(0u), m_maxCount(maxCount), m_dynamicIndexing(true) {
    uint32_t defIndex = 0u;
    uint32_t dstIndex = 0u;

    // Build mask of shader-defined constants
    small_vector<uint32_t, 64u> defMask(align(maxCount, 32u) / 32u);

    for (uint32_t i = 0u; i < defCount; i++) {
      if (defData[i].index < maxCount)
        defMask[defData[i].index / 32u] |= 1u << (defData[i].index % 32u);
    }

    // Find index of highest statically indexed constant
    for (uint32_t i = useLength; i; i--) {
      if (useMask[i - 1u]) {
        m_minCount = std::min(32u * i + bit::tzcnt(useMask[i - 1u]) - 31u, maxCount);
        break;
      }
    }

    // More or less the inverse of the above logic. Keep all constants intact,
    // but insert ranges for any constants that are shader-defined.
    for (uint32_t i = 0u; i < defMask.size(); i++) {
      uint32_t mask = defMask[i];

      while (mask) {
        uint32_t bitIndex = bit::tzcnt(mask);
        uint32_t bitCount = bit::tzcnt(mask + (1u << bitIndex)) - bitIndex;

        uint32_t defRangeStart = bitIndex + 32u * i;

        D3D9ConstantRange apiRange = {};
        apiRange.dstIndex = dstIndex;
        apiRange.srcIndex = dstIndex;
        apiRange.count = defRangeStart - dstIndex;
        apiRange.isDynamicallyIndexed = true;
        addRange(apiRange);

        D3D9ConstantRange defRange = {};
        defRange.dstIndex = defRangeStart;
        defRange.srcIndex = defIndex;
        defRange.count = bitCount;
        defRange.isDynamicallyIndexed = true;
        defRange.isShaderDefined = true;
        addRange(defRange);

        dstIndex = defRange.dstIndex + defRange.count;
        defIndex = defRange.srcIndex + defRange.count;

        addConstantData(defRange, defCount, defData);

        m_minCount = std::max(m_minCount, dstIndex);

        mask &= -2u << (bitIndex + bitCount - 1u);
      }
    }

    // Insert final range of constants
    D3D9ConstantRange range = {};
    range.dstIndex = dstIndex;
    range.srcIndex = dstIndex;
    range.count = maxCount - dstIndex;
    range.isDynamicallyIndexed = true;

    addRange(range);
  }

  D3D9ConstantBufferLayout::~D3D9ConstantBufferLayout() {

  }

  void D3D9ConstantBufferLayout::addRange(const D3D9ConstantRange& range) {
    if (!range.count)
      return;

    // Merge with previous range if possible
    if (!m_ranges.empty()) {
      auto& last = m_ranges.back();

      if (range.dstIndex == last.dstIndex + last.count
       && range.srcIndex == last.srcIndex + last.count
       && range.isDynamicallyIndexed == last.isDynamicallyIndexed
       && range.isShaderDefined == last.isShaderDefined) {
        last.count += range.count;
        return;
      }
    }

    m_ranges.push_back(range);
  }


  void D3D9ConstantBufferLayout::addConstantData(
    const D3D9ConstantRange&          range,
          size_t                      defCount,
    const D3D9ImmediateFloatConstant* defData) {
    for (uint32_t i = 0u; i < range.count; i++) {
      for (uint32_t j = 0u; j < defCount; j++) {
        if (defData[j].index == range.dstIndex + i) {
          m_constantData.push_back(defData[j].value);
          break;
        }
      }
    }
  }


  bool D3D9ConstantBufferLayout::eq(const D3D9ConstantBufferLayout& other) const {
    bool eq = m_minCount            == other.m_minCount
           && m_maxCount            == other.m_maxCount
           && m_dynamicIndexing     == other.m_dynamicIndexing
           && m_ranges.size()       == other.m_ranges.size()
           && m_constantData.size() == other.m_constantData.size();

    for (size_t i = 0u; i < m_ranges.size() && eq; i++)
      eq = m_ranges[i].eq(other.m_ranges[i]);

    for (size_t i = 0u; i < m_constantData.size() && eq; i++)
      eq = !std::memcmp(&m_constantData[i], &other.m_constantData[i], sizeof(Vector4));

    return eq;
  }


  size_t D3D9ConstantBufferLayout::hash() const {
    DxvkHashState hash;

    for (const auto& range : m_ranges)
      hash.add(range.hash());

    for (const auto& c : m_constantData) {
      for (uint32_t i = 0u; i < 4u; i++) {
        uint32_t dword = 0u;
        std::memcpy(&dword, &c[i], sizeof(dword));
        hash.add(dword);
      }
    }

    hash.add(m_minCount);
    hash.add(m_maxCount);
    hash.add(uint32_t(m_dynamicIndexing));
    return hash;
  }


  D3D9ConstantBufferCopy::D3D9ConstantBufferCopy() {

  }


  D3D9ConstantBufferCopy::D3D9ConstantBufferCopy(
          D3D9ConstantBufferLayout floats,
          D3D9ConstantBufferLayout ints,
          D3D9ConstantBufferLayout bools)
  : m_floatLayout (std::move(floats)),
    m_intLayout   (std::move(ints)),
    m_boolLayout  (std::move(bools)) { }


  D3D9ConstantBufferCopy::~D3D9ConstantBufferCopy() {

  }


  const D3D9ConstantBufferCopy* D3D9ConstantBufferCopy::getOrCreate(
          D3D9ConstantBufferLayout floats,
          D3D9ConstantBufferLayout ints,
          D3D9ConstantBufferLayout bools) {
    std::lock_guard lock(s_mutex);

    auto entry = s_layouts.emplace(
      std::move(floats),
      std::move(ints),
      std::move(bools));

    return &(*entry.first);
  }


  bool D3D9ConstantBufferCopy::eq(const D3D9ConstantBufferCopy& other) const {
    return m_floatLayout.eq(other.m_floatLayout)
        && m_intLayout.eq(other.m_intLayout)
        && m_boolLayout.eq(other.m_boolLayout);
  }


  size_t D3D9ConstantBufferCopy::hash() const {
    DxvkHashState hash;
    hash.add(m_floatLayout.hash());
    hash.add(m_intLayout.hash());
    hash.add(m_boolLayout.hash());
    return hash;
  }


  void D3D9ConstantBufferCopy::copyConstantData(const D3D9ConstantBufferCopyArgs& args) const {
    if (args.floatBufferSize) {
      if (unlikely(args.flushNan))
        writeFloatBuffer<true>(args);
      else
        writeFloatBuffer<false>(args);
    }

    if (args.intBufferSize)
      writeIntBuffer(args);

    if (args.boolBufferSize)
      writeBoolBuffer(args);
  }


  template<bool FlushNan>
  void D3D9ConstantBufferCopy::writeFloatBuffer(const D3D9ConstantBufferCopyArgs& args) const {
    uint32_t writeCount = m_floatLayout.computeConstantCount(args.floatConstantCount);
    uint32_t writeSize = writeCount * sizeof(Vector4);

    for (uint32_t i = 0u; i < m_floatLayout.getRangeCount(); i++)
      writeFloatRange<FlushNan>(args, m_floatLayout.getRange(i), writeCount);

    pad16(args.floatBuffer, writeSize, args.floatBufferSize);
  }


  template<bool FlushNan>
  void D3D9ConstantBufferCopy::writeFloatRange(const D3D9ConstantBufferCopyArgs& args, const D3D9ConstantRange& range, uint32_t maxCount) const {
    // Need to clamp constant count for the last dynamically indexed range
    auto dstPtr = reinterpret_cast<Vector4*>(args.floatBuffer) + range.dstIndex;
    auto dstCount = std::min<uint32_t>(range.count,
      maxCount - std::min<uint32_t>(range.dstIndex, maxCount));

    // Need to select the correct source array
    auto srcBuffer = range.isShaderDefined
      ? m_floatLayout.getShaderDefinedConstants()
      : args.constFloatApi;
    auto srcPtr = reinterpret_cast<const Vector4*>(srcBuffer) + range.srcIndex;

    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    uint32_t index = 0u;

    while (index + 2u <= dstCount) {
      auto src0 = _mm_loadu_ps(srcPtr[index + 0u].data);
      auto src1 = _mm_loadu_ps(srcPtr[index + 1u].data);

      if (FlushNan) {
        src0 = _mm_and_ps(src0, _mm_cmpeq_ps(src0, src0));
        src1 = _mm_and_ps(src1, _mm_cmpeq_ps(src1, src1));
      }

      _mm_stream_ps(dstPtr[index + 0u].data, src0);
      _mm_stream_ps(dstPtr[index + 1u].data, src1);

      index += 2u;
    }

    if (index < dstCount) {
      auto src0 = _mm_loadu_ps(srcPtr[index].data);

      if (FlushNan)
        src0 = _mm_and_ps(src0, _mm_cmpeq_ps(src0, src0));

      _mm_stream_ps(dstPtr[index].data, src0);
    }
    #else
    for (uint32_t i = 0u; i < dstCount; i++) {
      for (uint32_t j = 0u; j < 4u; j++) {
        float f = srcPtr[i][j];

        if (FlushNan)
          f = std::isnan(f) ? 0.0f : f;

        dstPtr[i][j] = f;
      }
    }
    #endif
  }


  void D3D9ConstantBufferCopy::writeIntBuffer(const D3D9ConstantBufferCopyArgs& args) const {
    uint32_t writeSize = m_intLayout.computeConstantCount(0u) * sizeof(Vector4i);

    for (uint32_t i = 0u; i < m_intLayout.getRangeCount(); i++)
      writeIntRange(args, m_intLayout.getRange(i));

    pad16(args.intBuffer, writeSize, args.intBufferSize);
  }


  void D3D9ConstantBufferCopy::writeIntRange(const D3D9ConstantBufferCopyArgs& args, const D3D9ConstantRange& range) const {
    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    auto dstPtr = reinterpret_cast<      __m128i*>(args.intBuffer) + range.dstIndex;
    auto srcPtr = reinterpret_cast<const __m128i*>(args.constIntApi) + range.srcIndex;

    for (uint32_t i = 0u; i < range.count; i++)
      _mm_stream_si128(dstPtr + i, _mm_loadu_si128(srcPtr + i));
    #else
    std::memcpy(reinterpret_cast<Vector4i*>(args.intBuffer) + range.dstIndex,
      args.constIntApi + range.srcIndex, range.count * sizeof(Vector4i));
    #endif
  }


  void D3D9ConstantBufferCopy::writeBoolBuffer(const D3D9ConstantBufferCopyArgs& args) const {
    uint32_t writeSize = m_boolLayout.computeConstantCount(0u) * sizeof(uint32_t);

    for (uint32_t i = 0u; i < m_boolLayout.getRangeCount(); i++)
      writeBoolRange(args, m_boolLayout.getRange(i));

    pad16(args.boolBuffer, writeSize, args.boolBufferSize);
  }


  void D3D9ConstantBufferCopy::writeBoolRange(const D3D9ConstantBufferCopyArgs& args, const D3D9ConstantRange& range) const {
    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    auto dstPtr = reinterpret_cast<      int*>(args.boolBuffer) + range.dstIndex;
    auto srcPtr = reinterpret_cast<const int*>(args.constBoolApi) + range.srcIndex;

    for (uint32_t i = 0u; i < range.count; i += 4u) {
      auto data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcPtr + i));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstPtr + i), data);
    }
    #else
    std::memcpy(reinterpret_cast<uint32_t*>(args.boolBuffer) + range.dstIndex,
      args.constBoolApi + range.srcIndex, range.count * sizeof(uint32_t));
    #endif
  }


  void D3D9ConstantBufferCopy::pad16(void* dst, size_t start, size_t end) {
    if (start < end) {
      #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
      auto dstPtr = reinterpret_cast<__m128i*>(reinterpret_cast<char*>(dst) + start);
      auto dstCount = (end - start) / sizeof(*dstPtr);

      __m128i zero = _mm_setzero_si128();

      for (uint32_t i = 0u; i < dstCount; i++)
        _mm_stream_si128(dstPtr + i, zero);
      #else
      std::memset(reinterpret_cast<char*>(dst) + start, 0, end - start);
      #endif
    }
  }

}
