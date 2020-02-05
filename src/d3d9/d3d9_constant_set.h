#pragma once

#include "d3d9_caps.h"

#include "../dxvk/dxvk_buffer.h"

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
  struct D3D9ShaderConstantsVSSoftware {
    Vector4  fConsts[caps::MaxFloatConstantsSoftware];
    Vector4i iConsts[caps::MaxOtherConstantsSoftware];
    uint32_t bConsts[caps::MaxOtherConstantsSoftware / 32];
  };

  struct D3D9ShaderConstantsVSHardware {
    Vector4  fConsts[caps::MaxFloatConstantsVS];
    Vector4i iConsts[caps::MaxOtherConstants];
    uint32_t bConsts[1];
  };

  struct D3D9ShaderConstantsPS {
    Vector4  fConsts[caps::MaxFloatConstantsPS];
    Vector4i iConsts[caps::MaxOtherConstants];
    uint32_t bConsts[1];
  };

  struct D3D9ConstantSets {
    Rc<DxvkBuffer>            buffer;
    DxsoShaderMetaInfo        meta  = {};
    bool                      dirty = true;
  };

}