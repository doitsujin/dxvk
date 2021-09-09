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
    Vector4i iConsts[caps::MaxOtherConstantsSoftware];
    Vector4  fConsts[caps::MaxFloatConstantsSoftware];
    uint32_t bConsts[caps::MaxOtherConstantsSoftware / 32];
  };

  struct D3D9ShaderConstantsVSHardware {
    Vector4i iConsts[caps::MaxOtherConstants];
    Vector4  fConsts[caps::MaxFloatConstantsVS];
    uint32_t bConsts[1];
  };

  struct D3D9ShaderConstantsPS {
    Vector4i iConsts[caps::MaxOtherConstants];
    Vector4  fConsts[caps::MaxFloatConstantsPS];
    uint32_t bConsts[1];
  };

  struct D3D9SwvpConstantBuffers {
    Rc<DxvkBuffer>        floatBuffer;
    Rc<DxvkBuffer>        intBuffer;
    Rc<DxvkBuffer>        boolBuffer;
  };

  struct D3D9ConstantSets {
    D3D9SwvpConstantBuffers   swvpBuffers;
    Rc<DxvkBuffer>            buffer;
    DxsoShaderMetaInfo        meta  = {};
    bool                      dirty = true;
  };

}