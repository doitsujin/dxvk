#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"

namespace dxvk::caps {

  HRESULT checkDeviceFormat(
    D3D9Format adapterFormat,
    DWORD usage,
    D3DRESOURCETYPE resourceType,
    D3D9Format checkFormat);

  HRESULT checkDepthStencilMatch(
    D3D9Format AdapterFormat,
    D3D9Format RenderTargetFormat,
    D3D9Format DepthStencilFormat);

  HRESULT checkDeviceFormatConversion(
    D3D9Format srcFormat,
    D3D9Format dstFormat);

  HRESULT checkDeviceMultiSampleType(
    D3D9Format SurfaceFormat,
    BOOL Windowed,
    D3DMULTISAMPLE_TYPE MultiSampleType,
    DWORD* pQualityLevels);

  HRESULT checkDeviceType(
    D3D9Format adapterFormat,
    D3D9Format backBufferFormat,
    BOOL windowed);

  HRESULT getDeviceCaps(UINT adapter, D3DDEVTYPE type, D3DCAPS9* pCaps);

  constexpr uint32_t MaxClipPlanes = 6;
  constexpr uint32_t MaxSamplers = 16;
  constexpr uint32_t MaxStreams = 16;
  constexpr uint32_t MaxTextureBlendStages = 8;
  constexpr uint32_t MaxSimultaneousRenderTargets = D3D_MAX_SIMULTANEOUS_RENDERTARGETS;
  constexpr uint32_t MaxFloatConstants = 256;
  constexpr uint32_t MaxOtherConstants = 16;

}