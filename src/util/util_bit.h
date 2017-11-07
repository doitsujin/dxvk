#pragma once

namespace dxvk::bit {
  
  template<typename T>
  T extract(T value, uint32_t fst, uint32_t lst) {
    return (value >> fst) & ~(~T(0) << (lst - fst + 1));
  }
  
  template<typename T>
  T popcnt(T value) {
    return value != 0
      ? (value & 1) + popcnt(value >> 1)
      : 0;
  }
  
  template<typename T>
  T tzcnt(T value) {
    uint32_t result = 0;
    while ((result < sizeof(T) * 8)
        && (((value >> result) & 1) == 0))
      result += 1;
    return result;
  }
  
}