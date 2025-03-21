#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_buffer.h"
#include "d3d9_constant_layout.h"


#include "../dxso/dxso_isgn.h"

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
    D3D9ConstantBuffer        intBuffer;
    D3D9ConstantBuffer        boolBuffer;
  };

  struct D3D9ConstantSets {
    D3D9ConstantLayout        layout;
    D3D9SwvpConstantBuffers   swvp;
    D3D9ConstantBuffer        buffer;
    DxsoShaderMetaInfo        meta  = {};
    bool                      dirty = true;
    uint32_t                  maxChangedConstF = 0;
    uint32_t                  maxChangedConstI = 0;
    uint32_t                  maxChangedConstB = 0;
  };

}
