#pragma once

#include "../dxvk/dxvk_device.h"
#include "../d3d9/d3d9_options.h"

namespace dxvk {

  class D3D9DeviceEx;
  struct D3D9Options;

  struct DxsoOptions {
    DxsoOptions();
    DxsoOptions(D3D9DeviceEx* pDevice, const D3D9Options& options);

    /// True:  Copy our constant set into UBO if we are relative indexing ever.
    /// False: Copy our constant set into UBO if we are relative indexing at the start of a defined constant
    /// Why?:  In theory, FXC should never generate code where this would be an issue.
    bool strictConstantCopies;

    /// Whether to emulate d3d9 float behaviour using clampps
    /// True:  Perform emulation to emulate behaviour (ie. anything * 0 = 0)
    /// False: Don't do anything.
    D3D9FloatEmulation d3d9FloatEmulation;

    /// Whether or not we should care about pow(0, 0) = 1
    bool strictPow;

    /// Work around a NV driver quirk
    /// Fixes flickering/z-fighting in some games.
    bool invariantPosition;

    /// Always use a spec constant to determine sampler type (instead of just in PS 1.x)
    /// Works around a game bug in Halo CE where it gives cube textures to 2d/volume samplers
    bool forceSamplerTypeSpecConstants;

    /// Interpolate pixel shader inputs at the sample location rather than pixel center
    bool forceSampleRateShading;

    /// Should the SWVP float constant buffer be a SSBO (because of the size on NV)
    bool vertexFloatConstantBufferAsSSBO;

    /// Whether or not we can rely on robustness2 to handle oob constant access
    bool robustness2Supported;

    /// Whether or not we need to use custom sincos
    bool sincosEmulation = false;

    /// Whether runtime to apply Dref scaling for depth textures of specified bit depth
    /// (24: D24S8, 16: D16, 0: Disabled). This allows compatability with games
    /// that expect a different depth test range, which was typically a D3D8 quirk on
    /// early NVIDIA hardware.
    int32_t drefScaling = 0;
  };

}
