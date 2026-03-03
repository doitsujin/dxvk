#pragma once

#include "../dxvk/dxvk_device.h"
#include "../d3d9/d3d9_options.h"

namespace dxvk {

  class D3D9DeviceEx;
  struct D3D9Options;

  struct DxsoOptions {
    DxsoOptions();
    DxsoOptions(D3D9DeviceEx* pDevice, const D3D9Options& options);

    /// Whether to emulate d3d9 float behaviour using clampps
    /// True:  Perform emulation to emulate behaviour (ie. anything * 0 = 0)
    /// False: Don't do anything.
    D3D9FloatEmulation d3d9FloatEmulation;

    /// Always use a spec constant to determine sampler type (instead of just in PS 1.x)
    /// Works around a game bug in Halo CE where it gives cube textures to 2d/volume samplers
    bool forceSamplerTypeSpecConstants;

    /// Interpolate pixel shader inputs at the sample location rather than pixel center
    bool forceSampleRateShading;

    /// Should the SWVP float constant buffer be a SSBO (because of the size on NV)
    bool vertexFloatConstantBufferAsSSBO;

    /// Whether or not we need to use custom sincos
    bool sincosEmulation = false;
  };

}
