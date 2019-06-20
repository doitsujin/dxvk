#pragma once

#include "d3d9_caps.h"

#include "../dxso/dxso_isgn.h"

#include "../util/util_math.h"
#include "../util/util_vector.h"

#include <cstdint>

namespace dxvk {

  enum class D3D9ConstantType {
    Float,
    Int,
    Bool
  };

  // We make an assumption later based on the packing of this struct for copying.
  struct D3D9ShaderConstants {
    struct alignas(32) {
      std::array<Vector4,  caps::MaxFloatConstants> fConsts;
      std::array<Vector4i, caps::MaxOtherConstants> iConsts;
      uint32_t boolBitfield = 0;
    } hardware;
  };

  struct D3D9ConstantSets {
    constexpr static uint32_t SetSize = sizeof(D3D9ShaderConstants);
    Rc<DxvkBuffer>            buffer;
    const DxsoShaderMetaInfo* meta  = nullptr;
    bool                      dirty = true;
  };

}