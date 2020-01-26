#pragma once

#include <cstdint>

namespace dxvk {

  enum D3D9SpecConstantId : uint32_t {
    AlphaTestEnable = 0,
    AlphaCompareOp  = 1,
    SamplerType     = 2,
    FogEnabled      = 3,
    VertexFogMode   = 4,
    PixelFogMode    = 5,

    PointMode       = 6,
    ProjectionType  = 7,

    VertexShaderBools = 8,
    PixelShaderBools  = 9,
  };

}