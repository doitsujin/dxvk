#pragma once

namespace dxvk {
  
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
  
}