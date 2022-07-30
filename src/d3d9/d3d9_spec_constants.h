#pragma once

#include <cstdint>

namespace dxvk {

  enum D3D9SpecConstantId : uint32_t {
    AlphaCompareOp  = 0,    // Range: 0 -> 7          | Bits: 3
    SamplerType     = 1,    // 2 bits for 16 samplers | Bits: 32
                            // ^ not used for vertex shaders
    FogEnabled      = 2,    // Range: 0 -> 1          | Bits: 1
    VertexFogMode   = 3,    // Range: 0 -> 3          | Bits: 2
    PixelFogMode    = 4,    // Range: 0 -> 3          | Bits: 2

    PointMode       = 5,    // Range: 0 -> 3          | Bits: 3
    ProjectionType  = 6,    // 1 bit for 6 samplers   | Bits: 6
                            // ^ not supported for vertex shaders
                            //   PS 1.x only supports up to 6 samplers

    VertexShaderBools = 7,  // 16 bools               | Bits: 16
    PixelShaderBools  = 8,  // 16 bools               | Bits: 16
    Fetch4            = 9,  // 1 bit for 16 samplers  | Bits: 16
                            // ^ not supported for vertex shaders

    SamplerDepthMode  = 10, // 1 bit for 20 samplers  | Bits: 20
                            // ^ vs + ps
    SamplerNull       = 11, // 1 bit for 20 samplers  | Bits: 20
                            // ^ vs + ps
  };

}