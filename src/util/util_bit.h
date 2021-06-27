#pragma once

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

#include "util_likely.h"
#include "util_math.h"

#include <cstring>
#include <type_traits>

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
    #elif defined(__GNUC__) || defined(__clang__)
    uint32_t res;
    uint32_t tmp;
    asm (
      "mov  $32, %1;"
      "bsf   %2, %0;"
      "cmovz %1, %0;"
      : "=&r" (res), "=&r" (tmp)
      : "r" (n));
    return res;
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
    #if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)
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

  private:

    uint32_t m_dwords[Dwords];

  };
  
}
