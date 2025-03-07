#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_buffer.h"

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
    D3D9CSConstantBuffer intBuffer;
    D3D9CSConstantBuffer boolBuffer;
  };

  template<typename ShaderConstantsStorage>
  struct D3D9CSShaderConstants {
    ShaderConstantsStorage constants;

    // Primary buffer (contains HWVP or pixel shaders: Ints + Floats, SWVP: Floats)
    D3D9CSConstantBuffer    buffer;
    // Secondary buffers for SWVP (one for Ints, one for Bools)
    D3D9SwvpConstantBuffers swvp;

    // Shader related
    DxsoShaderMetaInfo        meta  = {};
    DxsoDefinedConstants      shaderDefinedConsts;

    // Tracking
    bool                      dirty = true;
    uint32_t                  floatConstsCount = 0;
    // The highest changed int and bool constants are only tracked for SWVP.
    // For HWVP or pixel shaders, the maximum amount is only 16 anyway.
    uint32_t                  intConstsCount   = 0;
    uint32_t                  boolConstsCount = 0;
  };

}