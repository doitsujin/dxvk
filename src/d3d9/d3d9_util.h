#pragma once

#include "d3d9_include.h"

namespace dxvk {

  template <typename T>
  inline void forEachSampler(T func) {
    for (uint32_t i = 0; i <= D3DVERTEXTEXTURESAMPLER3; i = (i != 15) ? (i + 1) : D3DVERTEXTEXTURESAMPLER0)
      func(i);
  }

}