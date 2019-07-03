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
  struct D3D9ShaderConstantsVS {
    std::array<Vector4,  caps::MaxFloatConstantsVS> fConsts;
    std::array<Vector4i, caps::MaxOtherConstants>   iConsts;
    uint32_t boolBitfield = 0;
  };

  struct D3D9ShaderConstantsPS {
    std::array<Vector4,  caps::MaxFloatConstantsPS> fConsts;
    std::array<Vector4i, caps::MaxOtherConstants>   iConsts;
    uint32_t boolBitfield = 0;
  };

  struct D3D9ConstantSets {
    Rc<DxvkBuffer>            buffer;
    const DxsoShaderMetaInfo* meta  = nullptr;
    bool                      dirty = true;
  };

}