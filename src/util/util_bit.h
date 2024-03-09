#pragma once

#if (defined(__x86_64__) && !defined(__arm64ec__)) || (defined(_M_X64) && !defined(_M_ARM64EC)) \
    || defined(__i386__) || defined(_M_IX86) || defined(__e2k__)
  #define DXVK_ARCH_X86
  #if defined(__x86_64__) || defined(_M_X64) || defined(__e2k__)
    #define DXVK_ARCH_X86_64
  #endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
  #define DXVK_ARCH_ARM64
#else
#error "Unknown CPU Architecture"
#endif

#ifdef DXVK_ARCH_X86
  #ifndef _MSC_VER
    #if defined(__WINE__) && defined(__clang__)
      #pragma push_macro("_WIN32")
      #undef _WIN32
    #endif
    #include <x86intrin.h>
    #if defined(__WINE__) && defined(__clang__)
      #pragma pop_macro("_WIN32")
    #endif
  #else
    #include <intrin.h>
  #endif
#endif

#include "util_likely.h"
#include "util_math.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <vector>

namespace dxvk::bit {

  template<typename T, typename J>
  T cast(const J& src) {
    static_assert(sizeof(T) == sizeof(J));
    static_assert(std::is_trivially_copyable<J>::value && std::is_trivial<T>::value);

    T dst;
    std::memcpy(&dst, &src, sizeof(T));
    return dst;
  }
  
  template<typename T>
  T extract(T value, uint32_t fst, uint32_t lst) {
    return (value >> fst) & ~(~T(0) << (lst - fst + 1));
  }

  inline uint32_t popcntStep(uint32_t n, uint32_t mask, uint32_t shift) {
    return (n & mask) + ((n & ~mask) >> shift);
  }
  
  inline uint32_t popcnt(uint32_t n) {
    n = popcntStep(n, 0x55555555, 1);
    n = popcntStep(n, 0x33333333, 2);
    n = popcntStep(n, 0x0F0F0F0F, 4);
    n = popcntStep(n, 0x00FF00FF, 8);
    n = popcntStep(n, 0x0000FFFF, 16);
    return n;
  }
  
  inline uint32_t tzcnt(uint32_t n) {
    #if defined(_MSC_VER) && !defined(__clang__)
    return _tzcnt_u32(n);
    #elif defined(__BMI__)
    return __tzcnt_u32(n);
    #elif defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__))
    // tzcnt is encoded as rep bsf, so we can use it on all
    // processors, but the behaviour of zero inputs differs:
    // - bsf:   zf = 1, cf = ?, result = ?
    // - tzcnt: zf = 0, cf = 1, result = 32
    // We'll have to handle this case manually.
    uint32_t res;
    uint32_t tmp;
    asm (
      "tzcnt %2, %0;"
      "mov  $32, %1;"
      "test  %2, %2;"
      "cmovz %1, %0;"
      : "=&r" (res), "=&r" (tmp)
      : "r" (n)
      : "cc");
    return res;
    #elif defined(__GNUC__) || defined(__clang__)
    return n != 0 ? __builtin_ctz(n) : 32;
    #else
    uint32_t r = 31;
    n &= -n;
    r -= (n & 0x0000FFFF) ? 16 : 0;
    r -= (n & 0x00FF00FF) ?  8 : 0;
    r -= (n & 0x0F0F0F0F) ?  4 : 0;
    r -= (n & 0x33333333) ?  2 : 0;
    r -= (n & 0x55555555) ?  1 : 0;
    return n != 0 ? r : 32;
    #endif
  }

  inline uint32_t tzcnt(uint64_t n) {
    #if defined(DXVK_ARCH_X86_64) && defined(_MSC_VER) && !defined(__clang__)
    return (uint32_t)_tzcnt_u64(n);
    #elif defined(DXVK_ARCH_X86_64) && defined(__BMI__)
    return __tzcnt_u64(n);
    #elif defined(DXVK_ARCH_X86_64) && (defined(__GNUC__) || defined(__clang__))
    uint64_t res;
    uint64_t tmp;
    asm (
      "tzcnt %2, %0;"
      "mov  $64, %1;"
      "test  %2, %2;"
      "cmovz %1, %0;"
      : "=&r" (res), "=&r" (tmp)
      : "r" (n)
      : "cc");
    return res;
    #elif defined(__GNUC__) || defined(__clang__)
    return n != 0 ? __builtin_ctzll(n) : 64;
    #else
    uint32_t lo = uint32_t(n);
    if (lo) {
      return tzcnt(lo);
    } else {
      uint32_t hi = uint32_t(n >> 32);
      return tzcnt(hi) + 32;
    }
    #endif
  }

  inline uint32_t lzcnt(uint32_t n) {
    #if (defined(_MSC_VER) && !defined(__clang__)) || defined(__LZCNT__)
    return _lzcnt_u32(n);
    #elif defined(__GNUC__) || defined(__clang__)
    return n != 0 ? __builtin_clz(n) : 32;
    #else
    uint32_t r = 0;

    if (n == 0)	return 32;

    if (n <= 0x0000FFFF) { r += 16; n <<= 16; }
    if (n <= 0x00FFFFFF) { r += 8;  n <<= 8; }
    if (n <= 0x0FFFFFFF) { r += 4;  n <<= 4; }
    if (n <= 0x3FFFFFFF) { r += 2;  n <<= 2; }
    if (n <= 0x7FFFFFFF) { r += 1;  n <<= 1; }

    return r;
    #endif
  }

  template<typename T>
  uint32_t pack(T& dst, uint32_t& shift, T src, uint32_t count) {
    constexpr uint32_t Bits = 8 * sizeof(T);
    if (likely(shift < Bits))
      dst |= src << shift;
    shift += count;
    return shift > Bits ? shift - Bits : 0;
  }

  template<typename T>
  uint32_t unpack(T& dst, T src, uint32_t& shift, uint32_t count) {
    constexpr uint32_t Bits = 8 * sizeof(T);
    if (likely(shift < Bits))
      dst = (src >> shift) & ((T(1) << count) - 1);
    shift += count;
    return shift > Bits ? shift - Bits : 0;
  }

  /**
   * \brief Compares two aligned structs bit by bit
   *
   * \param [in] a First struct
   * \param [in] b Second struct
   * \returns \c true if the structs are equal
   */
  template<typename T>
  bool bcmpeq(const T* a, const T* b) {
    static_assert(alignof(T) >= 16);
    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    auto ai = reinterpret_cast<const __m128i*>(a);
    auto bi = reinterpret_cast<const __m128i*>(b);

    size_t i = 0;

    #if defined(__clang__)
    #pragma nounroll
    #elif defined(__GNUC__)
    #pragma GCC unroll 0
    #endif

    for ( ; i < 2 * (sizeof(T) / 32); i += 2) {
      __m128i eq0 = _mm_cmpeq_epi8(
        _mm_load_si128(ai + i),
        _mm_load_si128(bi + i));
      __m128i eq1 = _mm_cmpeq_epi8(
        _mm_load_si128(ai + i + 1),
        _mm_load_si128(bi + i + 1));
      __m128i eq = _mm_and_si128(eq0, eq1);

      int mask = _mm_movemask_epi8(eq);
      if (mask != 0xFFFF)
        return false;
    }

    for ( ; i < sizeof(T) / 16; i++) {
      __m128i eq = _mm_cmpeq_epi8(
        _mm_load_si128(ai + i),
        _mm_load_si128(bi + i));

      int mask = _mm_movemask_epi8(eq);
      if (mask != 0xFFFF)
        return false;
    }

    return true;
    #else
    return !std::memcmp(a, b, sizeof(T));
    #endif
  }

  template <size_t Bits>
  class bitset {
    static constexpr size_t Dwords = align(Bits, 32) / 32;
  public:

    constexpr bitset()
      : m_dwords() {

    }

    constexpr bool get(uint32_t idx) const {
      uint32_t dword = 0;
      uint32_t bit   = idx;

      // Compiler doesn't remove this otherwise.
      if constexpr (Dwords > 1) {
        dword = idx / 32;
        bit   = idx % 32;
      }

      return m_dwords[dword] & (1u << bit);
    }

    constexpr void set(uint32_t idx, bool value) {
      uint32_t dword = 0;
      uint32_t bit   = idx;

      // Compiler doesn't remove this otherwise.
      if constexpr (Dwords > 1) {
        dword = idx / 32;
        bit   = idx % 32;
      }

      if (value)
        m_dwords[dword] |= 1u << bit;
      else
        m_dwords[dword] &= ~(1u << bit);
    }

    constexpr bool exchange(uint32_t idx, bool value) {
      bool oldValue = get(idx);
      set(idx, value);
      return oldValue;
    }

    constexpr void flip(uint32_t idx) {
      uint32_t dword = 0;
      uint32_t bit   = idx;

      // Compiler doesn't remove this otherwise.
      if constexpr (Dwords > 1) {
        dword = idx / 32;
        bit   = idx % 32;
      }

      m_dwords[dword] ^= 1u << bit;
    }

    constexpr void setAll() {
      if constexpr (Bits % 32 == 0) {
        for (size_t i = 0; i < Dwords; i++)
          m_dwords[i] = std::numeric_limits<uint32_t>::max();
      }
      else {
        for (size_t i = 0; i < Dwords - 1; i++)
          m_dwords[i] = std::numeric_limits<uint32_t>::max();

        m_dwords[Dwords - 1] = (1u << (Bits % 32)) - 1;
      }
    }

    constexpr void clearAll() {
      for (size_t i = 0; i < Dwords; i++)
        m_dwords[i] = 0;
    }

    constexpr bool any() const {
      for (size_t i = 0; i < Dwords; i++) {
        if (m_dwords[i] != 0)
          return true;
      }

      return false;
    }

    constexpr uint32_t& dword(uint32_t idx) {
      return m_dwords[idx];
    }

    constexpr size_t bitCount() {
      return Bits;
    }

    constexpr size_t dwordCount() {
      return Dwords;
    }

    constexpr bool operator [] (uint32_t idx) const {
      return get(idx);
    }

    constexpr void setN(uint32_t bits) {
      uint32_t fullDwords = bits / 32;
      uint32_t offset = bits % 32;

      for (size_t i = 0; i < fullDwords; i++)
        m_dwords[i] = std::numeric_limits<uint32_t>::max();
     
      if (offset > 0)
        m_dwords[fullDwords] = (1u << offset) - 1;
    }

  private:

    uint32_t m_dwords[Dwords];

  };

  class bitvector {
  public:

    bool get(uint32_t idx) const {
      uint32_t dword = idx / 32;
      uint32_t bit   = idx % 32;

      return m_dwords[dword] & (1u << bit);
    }

    void ensureSize(uint32_t bitCount) {
      uint32_t dword = bitCount / 32;
      if (unlikely(dword >= m_dwords.size())) {
        m_dwords.resize(dword + 1);
      }
      m_bitCount = std::max(m_bitCount, bitCount);
    }

    void set(uint32_t idx, bool value) {
      ensureSize(idx + 1);

      uint32_t dword = 0;
      uint32_t bit   = idx;

      if (value)
        m_dwords[dword] |= 1u << bit;
      else
        m_dwords[dword] &= ~(1u << bit);
    }

    bool exchange(uint32_t idx, bool value) {
      ensureSize(idx + 1);

      bool oldValue = get(idx);
      set(idx, value);
      return oldValue;
    }

    void flip(uint32_t idx) {
      ensureSize(idx + 1);

      uint32_t dword = idx / 32;
      uint32_t bit   = idx % 32;

      m_dwords[dword] ^= 1u << bit;
    }

    void setAll() {
      if (m_bitCount % 32 == 0) {
        for (size_t i = 0; i < m_dwords.size(); i++)
          m_dwords[i] = std::numeric_limits<uint32_t>::max();
      }
      else {
        for (size_t i = 0; i < m_dwords.size() - 1; i++)
          m_dwords[i] = std::numeric_limits<uint32_t>::max();

        m_dwords[m_dwords.size() - 1] = (1u << (m_bitCount % 32)) - 1;
      }
    }

    void clearAll() {
      for (size_t i = 0; i < m_dwords.size(); i++)
        m_dwords[i] = 0;
    }

    bool any() const {
      for (size_t i = 0; i < m_dwords.size(); i++) {
        if (m_dwords[i] != 0)
          return true;
      }

      return false;
    }

    uint32_t& dword(uint32_t idx) {
      return m_dwords[idx];
    }

    size_t bitCount() const {
      return m_bitCount;
    }

    size_t dwordCount() const {
      return m_dwords.size();
    }

    bool operator [] (uint32_t idx) const {
      return get(idx);
    }

    void setN(uint32_t bits) {
      ensureSize(bits);

      uint32_t fullDwords = bits / 32;
      uint32_t offset = bits % 32;

      for (size_t i = 0; i < fullDwords; i++)
        m_dwords[i] = std::numeric_limits<uint32_t>::max();

      if (offset > 0)
        m_dwords[fullDwords] = (1u << offset) - 1;
    }

  private:

    std::vector<uint32_t> m_dwords;
    uint32_t              m_bitCount = 0;

  };

  class BitMask {

  public:

    class iterator {
    public:
      using iterator_category = std::input_iterator_tag;
      using value_type = uint32_t;
      using difference_type = uint32_t;
      using pointer = const uint32_t*;
      using reference = uint32_t;

      explicit iterator(uint32_t flags)
        : m_mask(flags) { }

      iterator& operator ++ () {
        m_mask &= m_mask - 1;
        return *this;
      }

      iterator operator ++ (int) {
        iterator retval = *this;
        m_mask &= m_mask - 1;
        return retval;
      }

      uint32_t operator * () const {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__BMI__) && defined(DXVK_ARCH_X86)
        uint32_t res;
        asm ("tzcnt %1,%0"
        : "=r" (res)
        : "r" (m_mask)
        : "cc");
        return res;
#else
        return tzcnt(m_mask);
#endif
      }

      bool operator == (iterator other) const { return m_mask == other.m_mask; }
      bool operator != (iterator other) const { return m_mask != other.m_mask; }

    private:

      uint32_t m_mask;

    };

    BitMask()
      : m_mask(0) { }

    BitMask(uint32_t n)
      : m_mask(n) { }

    iterator begin() {
      return iterator(m_mask);
    }

    iterator end() {
      return iterator(0);
    }

  private:

    uint32_t m_mask;

  };
}
