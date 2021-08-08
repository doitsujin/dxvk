#pragma once

#include <cstdint>

namespace dxvk {

  enum D3D9SpecConstantId : uint32_t {
    AlphaCompareOp  = 0,
    SamplerType     = 1,
    FogEnabled      = 2,
    VertexFogMode   = 3,
    PixelFogMode    = 4,

    PointMode       = 5,
    ProjectionType  = 6,

    VertexShaderBools = 7,
    PixelShaderBools  = 8,
    Fetch4            = 9,

    SamplerDepthMode  = 10,
  };

}