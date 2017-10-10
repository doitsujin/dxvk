#pragma once

namespace dxvk {
  
  template<typename T>
  T clamp(T n, T lo, T hi) {
    if (n < lo) return lo;
    if (n > hi) return hi;
    return n;
  }
  
}