#pragma once

#include <x86intrin.h>

namespace dxvk {
  
  constexpr size_t CACHE_LINE_SIZE = 64;
  
  template<typename T>
  T clamp(T n, T lo, T hi) {
    if (n < lo) return lo;
    if (n > hi) return hi;
    return n;
  }
  
  template<typename T, typename U = T>
  T align(T what, U to) {
    return (what + to - 1) & ~(to - 1);
  }
  
  inline uint32_t tzcnt(uint32_t n) {
    #if defined(__BMI__)
    return __tzcnt_u32(n);
    #elif defined(__GNUC__)
    uint32_t res;
    uint32_t tmp;
    asm (
      "xor %1, %1;"
      "bsf %2, %0;"
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
  
}