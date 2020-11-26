#pragma once

#include "../dxvk/dxvk_device.h"
#include "../d3d9/d3d9_options.h"

namespace dxvk {

  class D3D9DeviceEx;
  struct D3D9Options;

  struct DxsoOptions {
    DxsoOptions();
    DxsoOptions(D3D9DeviceEx* pDevice, const D3D9Options& options);

    /// Use a SPIR-V extension to implement D3D-style discards
    bool useDemoteToHelperInvocation = false;

    /// Use subgroup operations to discard fragment
    /// shader invocations if derivatives remain valid.
    bool useSubgroupOpsForEarlyDiscard = false;

    /// True:  Copy our constant set into UBO if we are relative indexing ever.
    /// False: Copy our constant set into UBO if we are relative indexing at the start of a defined constant
    /// Why?:  In theory, FXC should never generate code where this would be an issue.
    bool strictConstantCopies;

    /// Whether to emulate d3d9 float behaviour using clampps
    /// True:  Perform emulation to emulate behaviour (ie. anything * 0 = 0)
    /// False: Don't do anything.
    bool d3d9FloatEmulation;

    /// Whether or not we should care about pow(0, 0) = 1
    bool strictPow;

    /// Max version of shader to support
    uint32_t shaderModel;

    /// Work around a NV driver quirk
    /// Fixes flickering/z-fighting in some games.
    bool invariantPosition;

    /// Always use a spec constant to determine sampler type (instead of just in PS 1.x)
    /// Works around a game bug in Halo CE where it gives cube textures to 2d/volume samplers
    bool forceSamplerTypeSpecConstants;

    /// Should the VS constant buffer be an SSBO (swvp on NV)
    bool vertexConstantBufferAsSSBO;

    /// Should we make our Mads a FFma or do it the long way with an FMul and an FAdd?
    /// This solves some rendering bugs in games that have z-pass shaders which
    /// don't match entirely to the regular vertex shader in this way.
    bool longMad;

    /// Workaround for games using alpha test == 1.0, etc due to wonky interpolation or
    /// misc. imprecision on some vendors
    bool alphaTestWiggleRoom;
  };

}