#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"

namespace dxvk {
  struct D3D9Options;
}

namespace dxvk::caps {

  bool    IsDepthFormat(D3D9Format Format);

  HRESULT CheckDeviceFormat(
          D3D9Format      AdapterFormat,
          DWORD           Usage,
          D3DRESOURCETYPE ResourceType,
          D3D9Format      CheckFormat);

  HRESULT CheckDepthStencilMatch(
          D3D9Format AdapterFormat,
          D3D9Format RenderTargetFormat,
          D3D9Format DepthStencilFormat);

  HRESULT CheckDeviceFormatConversion(
          D3D9Format SrcFormat,
          D3D9Format DstFormat);

  HRESULT CheckDeviceMultiSampleType(
          D3D9Format          SurfaceFormat,
          BOOL                Windowed,
          D3DMULTISAMPLE_TYPE MultiSampleType,
          DWORD*              pQualityLevels);

  HRESULT CheckDeviceType(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed);

  HRESULT GetDeviceCaps(
    const dxvk::D3D9Options& Options,
          UINT               Adapter,
          D3DDEVTYPE         Type,
          D3DCAPS9*          pCaps);

  constexpr uint32_t MaxClipPlanes                = 6;
  constexpr uint32_t MaxSamplers                  = 16;
  constexpr uint32_t MaxStreams                   = 16;
  constexpr uint32_t MaxSimultaneousTextures      = 8;
  constexpr uint32_t MaxTextureBlendStages        = MaxSimultaneousTextures;
  constexpr uint32_t MaxSimultaneousRenderTargets = D3D_MAX_SIMULTANEOUS_RENDERTARGETS;

  constexpr uint32_t MaxFloatConstantsVS          = 256;
  constexpr uint32_t MaxFloatConstantsPS          = 224;
  constexpr uint32_t MaxOtherConstants            = 16;
  constexpr uint32_t MaxFloatConstantsSoftware    = 8192;
  constexpr uint32_t MaxOtherConstantsSoftware    = 2048;

  constexpr uint32_t InputRegisterCount           = 16;

  constexpr uint32_t MaxTextureDimension          = 16384;
  constexpr uint32_t MaxMipLevels                 = 15;
  constexpr uint32_t MaxSubresources              = 15 * 6;

  constexpr uint32_t MaxTransforms                = 10 + 256;

  constexpr uint32_t TextureStageCount           = MaxSimultaneousTextures;

}