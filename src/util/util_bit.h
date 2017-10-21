#pragma once

namespace dxvk::bit {
  
  template<typename T>
  inline T extract(T value, uint32_t fst, uint32_t lst) {
    return (value >> fst) & ~(~T(0) << (lst - fst + 1));
  }
  
}