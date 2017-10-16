#pragma once

namespace dxvk::bit {
  
  template<typename T, T Fst, T Lst>
  constexpr T extract(T value) {
    return (value >> Fst) & ~(~T(0) << (Lst - Fst + 1));
  }
  
}