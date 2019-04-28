#include "d3d9_caps.h"

#include "d3d9_monitor.h"
#include "d3d9_format.h"
#include "d3d9_options.h"

#include <cfloat>

namespace dxvk::caps {

  bool IsDepthFormat(D3D9Format Format) {
    return Format == D3D9Format::D16_LOCKABLE
        || Format == D3D9Format::D32
        || Format == D3D9Format::D15S1
        || Format == D3D9Format::D24S8
        || Format == D3D9Format::D24X8
        || Format == D3D9Format::D24X4S4
        || Format == D3D9Format::D16
        || Format == D3D9Format::D32F_LOCKABLE
        || Format == D3D9Format::D24FS8
        || Format == D3D9Format::D32_LOCKABLE
        || Format == D3D9Format::DF16
        || Format == D3D9Format::DF24
        || Format == D3D9Format::INTZ;
  }

  HRESULT CheckDeviceFormat(
          D3D9Format      AdapterFormat,
          DWORD           Usage,
          D3DRESOURCETYPE ResourceType,
          D3D9Format      CheckFormat) {
    if (!IsSupportedMonitorFormat(AdapterFormat, FALSE))
      return D3DERR_NOTAVAILABLE;

    const bool dmap = Usage & D3DUSAGE_DMAP;
    const bool rt   = Usage & D3DUSAGE_RENDERTARGET;
    const bool ds   = Usage & D3DUSAGE_DEPTHSTENCIL;

    const bool surface = ResourceType == D3DRTYPE_SURFACE;
    const bool texture = ResourceType == D3DRTYPE_TEXTURE;

    const bool twoDimensional = surface || texture;

    const bool srgb = (Usage & (D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE)) != 0;

    if (CheckFormat == D3D9Format::INST)
      return D3D_OK;

    if (ds && !IsDepthFormat(CheckFormat))
      return D3DERR_NOTAVAILABLE;

    if (rt && CheckFormat == D3D9Format::NULL_FORMAT && twoDimensional)
      return D3D_OK;

    if (CheckFormat == D3D9Format::ATOC && surface)
      return D3D_OK;

    // I really don't want to support this...
    if (dmap)
      return D3DERR_NOTAVAILABLE;

    auto mapping = ConvertFormatUnfixed(CheckFormat);
    if (mapping.Format     == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    if (mapping.FormatSrgb == VK_FORMAT_UNDEFINED && srgb)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }

  HRESULT CheckDepthStencilMatch(
          D3D9Format AdapterFormat,
          D3D9Format RenderTargetFormat,
          D3D9Format DepthStencilFormat) {
    if (!IsSupportedMonitorFormat(AdapterFormat, FALSE))
      return D3DERR_NOTAVAILABLE;

    if (!IsDepthFormat(DepthStencilFormat))
      return D3DERR_NOTAVAILABLE;

    auto mapping = ConvertFormatUnfixed(RenderTargetFormat);
    if (mapping.Format == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }

  HRESULT CheckDeviceFormatConversion(
          D3D9Format SrcFormat,
          D3D9Format DstFormat) {
    auto src = ConvertFormatUnfixed(SrcFormat);
    auto dst = ConvertFormatUnfixed(DstFormat);
    if (src.Format == VK_FORMAT_UNDEFINED
     || dst.Format == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }

  HRESULT CheckDeviceMultiSampleType(
        D3D9Format          SurfaceFormat,
        BOOL                Windowed,
        D3DMULTISAMPLE_TYPE MultiSampleType,
        DWORD*              pQualityLevels) {
    if (pQualityLevels != nullptr)
      *pQualityLevels = 1;

    auto dst = ConvertFormatUnfixed(SurfaceFormat);
    if (dst.Format == VK_FORMAT_UNDEFINED)
      return D3DERR_NOTAVAILABLE;

    if (SurfaceFormat == D3D9Format::D32_LOCKABLE
     || SurfaceFormat == D3D9Format::D32F_LOCKABLE
     || SurfaceFormat == D3D9Format::D16_LOCKABLE)
      return D3DERR_NOTAVAILABLE;

    // Not a multiple of 2
    // Not nonmaskable
    // Not greater than 8
    if ((MultiSampleType % 2 != 0 && MultiSampleType != 1)
      || MultiSampleType > 8)
      return D3DERR_NOTAVAILABLE;

    if (pQualityLevels != nullptr) {
      if (MultiSampleType == D3DMULTISAMPLE_NONMASKABLE)
        *pQualityLevels = 4;
      else
        *pQualityLevels = 1;
    }

    return D3D_OK;
  }

  HRESULT CheckDeviceType(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed) {
    if (!IsSupportedBackBufferFormat(
      AdapterFormat, BackBufferFormat, Windowed))
      return D3DERR_NOTAVAILABLE;

    return D3D_OK;
  }

  HRESULT GetDeviceCaps(
    const dxvk::D3D9Options& Options,
          UINT               Adapter,
          D3DDEVTYPE         Type,
          D3DCAPS9*          pCaps) {
    if (pCaps == nullptr)
      return D3DERR_INVALIDCALL;

    // Based off what I get from native D3D.
    // TODO: Make this more based in reality of the adapter.

    pCaps->DeviceType = Type;
    pCaps->AdapterOrdinal = Adapter;
    pCaps->Caps = 131072;
    pCaps->Caps2 = 3758227456;
    pCaps->Caps3 = 928;
    pCaps->PresentationIntervals = 2147483663;
    pCaps->CursorCaps = 1;
    pCaps->DevCaps = 1818352;
    pCaps->PrimitiveMiscCaps = 4181234;
    pCaps->RasterCaps = 259219857;
    pCaps->ZCmpCaps = 255;
    pCaps->SrcBlendCaps = 16383;
    pCaps->DestBlendCaps = 16383;
    pCaps->AlphaCmpCaps = 255;
    pCaps->ShadeCaps = 541192;
    pCaps->TextureCaps = 126021;
    pCaps->TextureFilterCaps = 117638912;
    pCaps->CubeTextureFilterCaps = 50529024;
    pCaps->VolumeTextureFilterCaps = 117638912;
    pCaps->TextureAddressCaps = 63;
    pCaps->VolumeTextureAddressCaps = 63;
    pCaps->LineCaps = 31;
    pCaps->MaxTextureWidth = MaxTextureDimension;
    pCaps->MaxTextureHeight = MaxTextureDimension;
    pCaps->MaxVolumeExtent = 8192;
    pCaps->MaxTextureRepeat = 8192;
    pCaps->MaxTextureAspectRatio = 8192;
    pCaps->MaxAnisotropy = 16;
    pCaps->MaxVertexW = 1e10f;
    pCaps->GuardBandLeft = -32768.0f;
    pCaps->GuardBandTop = -32768.0f;
    pCaps->GuardBandRight = 32768.0f;
    pCaps->GuardBandBottom = 32768.0f;
    pCaps->ExtentsAdjust = 0.0f;
    pCaps->StencilCaps = 511;
    pCaps->FVFCaps = 1048584;
    pCaps->TextureOpCaps = 67108863;
    pCaps->MaxTextureBlendStages = MaxTextureBlendStages;

    pCaps->MaxSimultaneousTextures = 8;

    pCaps->VertexProcessingCaps = 379;
    pCaps->MaxActiveLights = 8;
    pCaps->MaxUserClipPlanes = MaxClipPlanes;
    pCaps->MaxVertexBlendMatrices = 4;
    pCaps->MaxVertexBlendMatrixIndex = 0;
    pCaps->MaxPointSize = 256.0f;
    pCaps->MaxPrimitiveCount = 5592405;
    pCaps->MaxVertexIndex = 16777215;
    pCaps->MaxStreams = MaxStreams;
    pCaps->MaxStreamStride = 508;

    if      (Options.shaderModel == 3) {
      pCaps->VertexShaderVersion = D3DVS_VERSION(3, 0);
      pCaps->PixelShaderVersion  = D3DPS_VERSION(3, 0);
    }
    else if (Options.shaderModel == 2) {
      pCaps->VertexShaderVersion = D3DVS_VERSION(2, 0);
      pCaps->PixelShaderVersion  = D3DPS_VERSION(2, 0);
    }
    else if (Options.shaderModel == 1) {
      pCaps->VertexShaderVersion = D3DVS_VERSION(1, 4);
      pCaps->PixelShaderVersion  = D3DPS_VERSION(1, 4);
    }

    pCaps->MaxVertexShaderConst = MaxFloatConstants;
    pCaps->PixelShader1xMaxValue = FLT_MAX;
    pCaps->DevCaps2 = 113;
    pCaps->MaxNpatchTessellationLevel = 1.0f;
    pCaps->Reserved5 = 0;
    pCaps->MasterAdapterOrdinal = 0;
    pCaps->AdapterOrdinalInGroup = 0;
    pCaps->NumberOfAdaptersInGroup = 2;
    pCaps->DeclTypes = 1023;
    pCaps->NumSimultaneousRTs = MaxSimultaneousRenderTargets;
    pCaps->StretchRectFilterCaps = 50332416;

    pCaps->VS20Caps.Caps = 1;
    pCaps->VS20Caps.DynamicFlowControlDepth = 24;
    pCaps->VS20Caps.NumTemps = 32;
    pCaps->VS20Caps.StaticFlowControlDepth = 4;

    pCaps->PS20Caps.Caps = 31;
    pCaps->PS20Caps.DynamicFlowControlDepth = 24;
    pCaps->PS20Caps.NumTemps = 32;
    pCaps->PS20Caps.StaticFlowControlDepth = 4;

    if (Options.shaderModel >= 2)
      pCaps->PS20Caps.NumInstructionSlots = 512;
    else
      pCaps->PS20Caps.NumInstructionSlots = 256;

    pCaps->VertexTextureFilterCaps = 50332416;
    pCaps->MaxVShaderInstructionsExecuted = 4294967295;
    pCaps->MaxPShaderInstructionsExecuted = 4294967295;

    if (Options.shaderModel == 3) {
      pCaps->MaxVertexShader30InstructionSlots = 32768;
      pCaps->MaxPixelShader30InstructionSlots = 32768;
    }
    else {
      pCaps->MaxVertexShader30InstructionSlots = 0;
      pCaps->MaxPixelShader30InstructionSlots = 0;
    }

    return D3D_OK;
  }

}
