#include "d3d9_caps.h"

namespace dxvk {
  void FillCaps(UINT adapter, D3DCAPS9& caps) {
    // All Vulkan-capable devices already support pretty much all the D3D9 features,
    // which is why we fill this out without checking for support.

    // First, zero this structure out.
    caps = D3DCAPS9{};

    caps.DeviceType = D3DDEVTYPE_HAL;
    caps.AdapterOrdinal = adapter;

    caps.Caps = 0;

    caps.Caps2 = D3DCAPS2_CANAUTOGENMIPMAP
      | D3DCAPS2_CANCALIBRATEGAMMA
      | D3DCAPS2_FULLSCREENGAMMA
      // TODO: D3D9Ex only: D3DCAPS2_CANSHARERESOURCE
      | D3DCAPS2_CANMANAGERESOURCE
      | D3DCAPS2_DYNAMICTEXTURES;

    caps.Caps3 = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD
      | D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM
      // TODO: D3D9Ex D3DCAPS3_DXVAHD
      | D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION;

    // Enable all present intervals.
    caps.PresentationIntervals = D3DPRESENT_INTERVAL_IMMEDIATE | 0b1111;

    caps.CursorCaps = D3DCURSORCAPS_COLOR
      | D3DCURSORCAPS_LOWRES;

    caps.DevCaps = D3DDEVCAPS_CANBLTSYSTONONLOCAL
      | D3DDEVCAPS_CANRENDERAFTERFLIP
      // These flags indicate hardware which is at least DirectX 5 / 7 compatible.
      | D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_DRAWPRIMITIVES2EX
      | D3DDEVCAPS_DRAWPRIMTLVERTEX
      // Not sure what these flags indicate, but we probably support them anyhow.
      | D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY
      | D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_HWTRANSFORMANDLIGHT
      | D3DDEVCAPS_PUREDEVICE
      /*
      TODO: determine what these refer to, and enable them if possible.
      | D3DDEVCAPS_NPATCHES | D3DDEVCAPS_QUINTICRTPATCHES
      | D3DDEVCAPS_RTPATCHES | D3DDEVCAPS_RTPATCHHANDLEZERO
      */
      | D3DDEVCAPS_SEPARATETEXTUREMEMORIES | D3DDEVCAPS_TEXTURENONLOCALVIDMEM
      | D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY
      | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    caps.PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ
      // In theory we could support both cull modes,
      // but it's better to just use the default one.
      | D3DPMISCCAPS_CULLCW
      | D3DPMISCCAPS_COLORWRITEENABLE
      | D3DPMISCCAPS_CLIPPLANESCALEDPOINTS
      | D3DPMISCCAPS_CLIPTLVERTS
      | D3DPMISCCAPS_BLENDOP
      // Modern hardware supports using textures in all stages.
      | D3DPMISCCAPS_TSSARGTEMP
      | D3DPMISCCAPS_INDEPENDENTWRITEMASKS
      | D3DPMISCCAPS_PERSTAGECONSTANT
      | D3DPMISCCAPS_POSTBLENDSRGBCONVERT
      | D3DPMISCCAPS_FOGANDSPECULARALPHA
      | D3DPMISCCAPS_SEPARATEALPHABLEND
      | D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS
      | D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING;

    caps.RasterCaps = D3DPRASTERCAPS_ANISOTROPY
      | D3DPRASTERCAPS_COLORPERSPECTIVE
      // We don't need dithering on modern displays,
      // but it doesn't hurt to advertise support for this.
      | D3DPRASTERCAPS_DITHER
      | D3DPRASTERCAPS_DEPTHBIAS
      | D3DPRASTERCAPS_FOGRANGE
      | D3DPRASTERCAPS_FOGVERTEX
      | D3DPRASTERCAPS_MIPMAPLODBIAS
      // TODO: We can't really support this efficiently on D3D11.
      // Don't know if any games would require this to be emulated.
      // D3DPRASTERCAPS_MULTISAMPLE_TOGGLE
      | D3DPRASTERCAPS_SCISSORTEST
      | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS
      // TODO: Not sure if we can support the ones below:
      //| D3DPRASTERCAPS_WBUFFER
      //| D3DPRASTERCAPS_WFOG
      | D3DPRASTERCAPS_ZBUFFERLESSHSR
      | D3DPRASTERCAPS_ZFOG
      // Depth queries.
      | D3DPRASTERCAPS_ZTEST;

    // We support everything, basically.
    caps.AlphaCmpCaps = caps.ZCmpCaps = (1 << 8) - 1;

    // Modern hardware supports mostly everything.
    caps.DestBlendCaps = caps.SrcBlendCaps = (1 << 16) - 1;

    caps.ShadeCaps = D3DPSHADECAPS_ALPHAGOURAUDBLEND
      | D3DPSHADECAPS_COLORGOURAUDRGB
      | D3DPSHADECAPS_FOGGOURAUD
      | D3DPSHADECAPS_SPECULARGOURAUDRGB;

    // Support for everything
    caps.TextureCaps = ((1 << 22) - 1)
      // This cap indicates lack of support, so we mask it.
      & (~D3DPTEXTURECAPS_NOPROJECTEDBUMPENV);

    // All the filters.
    caps.TextureFilterCaps = caps.CubeTextureFilterCaps =
      caps.VolumeTextureFilterCaps = (1 << 29) - 1;

    caps.TextureAddressCaps =
      caps.VolumeTextureAddressCaps = (1 << 6) - 1;

    // All the features.
    caps.LineCaps = (1 << 6) - 1;

    // The OpenGL 4.1 spec guarantees this minimum max texture size.
    caps.MaxTextureWidth = caps.MaxTextureHeight = caps.MaxVolumeExtent = 16384;

    // Not sure what this maximum should be, just leave the maximum possible.
    caps.MaxTextureRepeat = std::numeric_limits<unsigned int>::max();

    // Max ratio would be 16384 by 1.
    caps.MaxTextureAspectRatio = 16384;

    caps.MaxAnisotropy = 16;

    caps.MaxVertexW = 16777216.f;

    caps.GuardBandLeft = 0;
    caps.GuardBandTop = 0;
    caps.GuardBandRight = 0;
    caps.GuardBandBottom = 0;

    caps.ExtentsAdjust = 0;

    caps.StencilCaps = (1 << 9) - 1;

    // We support flexible vertex format capabilities.
    caps.FVFCaps = D3DFVFCAPS_PSIZE
      // This indicates we support up to 8 texture coordinates in a vertex shader.
      | (((1 << 16) - 1) & D3DFVFCAPS_TEXCOORDCOUNTMASK);

    // Enable all the bits.
    caps.TextureOpCaps = (~0);

    // These are pretty much unlimited on modern hardware,
    // so we set some very big numbers here to indicate that.
    caps.MaxTextureBlendStages = caps.MaxSimultaneousTextures =
      caps.MaxActiveLights = caps.MaxUserClipPlanes = caps.MaxVertexBlendMatrices =
      caps.MaxVertexBlendMatrixIndex = (1 << 31);

    caps.VertexProcessingCaps = (1 << 9) - 1;

    // We've no way to query how big this is on modern hardware,
    // but it's safe to assume it's pretty big.
    caps.MaxPointSize = 16384.f;

    caps.MaxPrimitiveCount = caps.MaxVertexIndex = std::numeric_limits<unsigned int>::max();

    // Valid range is 1 through 16, we expose the max.
    caps.MaxStreams = 16;

    // Some large power of two.
    caps.MaxStreamStride = 1 << 31;

    caps.VertexShaderVersion = D3DVS_VERSION(3, 0);
    // This the max you can get in D3D11.
    caps.MaxVertexShaderConst = 1 << 16;

    caps.PixelShaderVersion = D3DPS_VERSION(3, 0);
    caps.PixelShader1xMaxValue = 65536.f;

    // All of the caps!
    caps.DevCaps2 = (1 << 7) - 1;

    caps.MaxNpatchTessellationLevel = 256;

    // We don't support adapter groups / multi-GPU configs.
    // Just report each GPU as independent.
    caps.MasterAdapterOrdinal = adapter;
    caps.NumberOfAdaptersInGroup = 1;
    caps.AdapterOrdinalInGroup = 0;

    // Support all types.
    caps.DeclTypes = (1 << 10) - 1;

    // There is no real limit on modern GPUs, except for available VRAM.
    // Limit this to a reasonable number.
    caps.NumSimultaneousRTs = 64;

    // All the possible filters.
    caps.StretchRectFilterCaps = (~0);

    auto& vsc = caps.VS20Caps;
    vsc.Caps = ~0;
    vsc.DynamicFlowControlDepth = 24;
    vsc.NumTemps = 16384;
    // Practically infinite, just give a nice big number here.
    vsc.StaticFlowControlDepth = 1 << 24;

    auto& psc = caps.PS20Caps;
    psc.Caps = ~0;
    psc.DynamicFlowControlDepth = 24;
    psc.NumTemps = 16384;
    psc.StaticFlowControlDepth = 1 << 24;
    psc.NumInstructionSlots = 1 << 31;

    caps.VertexTextureFilterCaps = ~0;

    caps.MaxVShaderInstructionsExecuted = std::numeric_limits<unsigned int>::max();
    caps.MaxPShaderInstructionsExecuted = std::numeric_limits<unsigned int>::max();

    // Set this to the max possible value.
    caps.MaxVertexShader30InstructionSlots = 32768;
    caps.MaxPixelShader30InstructionSlots = 32768;
  }
}
