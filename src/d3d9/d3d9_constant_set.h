#pragma once

#include "d3d9_caps.h"

#include "../util/util_math.h"

#include <cstdint>

namespace dxvk {

  enum class D3D9ConstantType {
    Float,
    Int,
    Bool
  };

  // We make an assumption later based on the packing of this struct for copying.
  struct D3D9ShaderConstants {
    using vec4  = std::array<float, 4>;
    using ivec4 = std::array<int,   4>;

    struct alignas(32) {
      std::array<vec4,  caps::MaxFloatConstants> fConsts = { 0.0f };
      std::array<ivec4, caps::MaxOtherConstants> iConsts = { 0 };
      uint32_t boolBitfield = 0;
    } hardware;
  };

  struct D3D9ConstantSets {
    constexpr static uint32_t SetSize    = sizeof(D3D9ShaderConstants);
    Rc<DxvkBuffer> buffer;
    bool           dirty = true;
  };

}