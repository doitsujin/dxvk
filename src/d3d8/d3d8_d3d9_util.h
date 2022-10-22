#pragma once

/**
* Utility functions for converting
* between DirectX8 and DirectX9 types.
*/

#include "d3d8_include.h"

namespace dxvk {

  // (8<-9) D3DCAPSX: Writes to D3DCAPS8 from D3DCAPS9
  inline void ConvertCaps8(const d3d9::D3DCAPS9& caps9, D3DCAPS8* pCaps8) {

    // should be aligned
    std::memcpy(pCaps8, &caps9, sizeof(D3DCAPS8));

    // Max supported shader model is 1.4
    pCaps8->VertexShaderVersion = D3DVS_VERSION(1, 4);
    pCaps8->PixelShaderVersion  = D3DPS_VERSION(1, 4);

    // This was removed by D3D9. We can probably render windowed.
    pCaps8->Caps2 |= D3DCAPS2_CANRENDERWINDOWED;
  }

  // (9<-8) D3DD3DPRESENT_PARAMETERS: Returns D3D9's params given an input for D3D8
  inline d3d9::D3DPRESENT_PARAMETERS ConvertPresentParameters9(const D3DPRESENT_PARAMETERS* pParams) {

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth = pParams->BackBufferWidth;
    params.BackBufferHeight = pParams->BackBufferHeight;
    params.BackBufferFormat = d3d9::D3DFORMAT(pParams->BackBufferFormat);
    params.BackBufferCount = pParams->BackBufferCount;

    params.MultiSampleType = d3d9::D3DMULTISAMPLE_TYPE(pParams->MultiSampleType);
    params.MultiSampleQuality = 0; // (D3D8: no MultiSampleQuality), TODO: get a value for this


    UINT PresentationInterval = pParams->FullScreen_PresentationInterval;

    if (pParams->Windowed) {

      if (unlikely(PresentationInterval != D3DPRESENT_INTERVAL_DEFAULT)) {
        // TODO: what does dx8 do if windowed app sets FullScreen_PresentationInterval?
        Logger::warn(str::format(
          "D3D8 Application is windowed yet requested FullScreen_PresentationInterval ", PresentationInterval,
          " (should be D3DPRESENT_INTERVAL_DEFAULT). This will be ignored."));
      }

      // D3D8: For windowed swap chain, the back buffer is copied to the window immediately.
      PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    }

    D3DSWAPEFFECT SwapEffect = pParams->SwapEffect;

    // D3DSWAPEFFECT_COPY_VSYNC has been removed
    if (SwapEffect == D3DSWAPEFFECT_COPY_VSYNC) {

      SwapEffect = D3DSWAPEFFECT_COPY;

      // D3D8: In windowed mode, D3DSWAPEFFECT_COPY_VSYNC enables VSYNC.
      // In fullscreen, D3DPRESENT_INTERVAL_IMMEDIATE is meaningless.
      if (pParams->Windowed || (PresentationInterval & D3DPRESENT_INTERVAL_IMMEDIATE) != 0) {
        PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        // TODO: what does dx8 do if multiple D3DPRESENT_INTERVAL flags are set? 
      }
    }

    params.SwapEffect = d3d9::D3DSWAPEFFECT(SwapEffect);
    params.hDeviceWindow = pParams->hDeviceWindow;
    params.Windowed = pParams->Windowed;
    params.EnableAutoDepthStencil = pParams->EnableAutoDepthStencil;
    params.AutoDepthStencilFormat = d3d9::D3DFORMAT(pParams->AutoDepthStencilFormat);
    params.Flags = pParams->Flags;

    params.FullScreen_RefreshRateInHz = pParams->FullScreen_RefreshRateInHz;

    // FullScreen_PresentationInterval -> PresentationInterval
    params.PresentationInterval = PresentationInterval;

    return params;
  }

  inline UINT GetFormatBPP(const D3DFORMAT fmt) {
    // TODO: get bpp based on format
    return 32;
  }

  // (8<-9) Convert D3DSURFACE_DESC
  inline void ConvertSurfaceDesc8(const d3d9::D3DSURFACE_DESC* pSurf9, D3DSURFACE_DESC* pSurf8) {
    pSurf8->Format  = D3DFORMAT(pSurf9->Format);
    pSurf8->Type    = D3DRESOURCETYPE(pSurf9->Type);
    pSurf8->Usage   = pSurf9->Usage;
    pSurf8->Pool    = D3DPOOL(pSurf9->Pool);
    pSurf8->Size    = pSurf9->Width * pSurf9->Height * GetFormatBPP(pSurf8->Format);

    pSurf8->MultiSampleType = D3DMULTISAMPLE_TYPE(pSurf9->MultiSampleType);
    // DX8: No multisample quality
    pSurf8->Width   = pSurf9->Width;
    pSurf8->Height  = pSurf9->Height;
  }

  // (8<-9) Convert D3DVOLUME_DESC
  inline void ConvertVolumeDesc8(const d3d9::D3DVOLUME_DESC* pVol9, D3DVOLUME_DESC* pVol8) {
    pVol8->Format = D3DFORMAT(pVol9->Format);
    pVol8->Type   = D3DRESOURCETYPE(pVol9->Type);
    pVol8->Usage  = pVol9->Usage;
    pVol8->Pool   = D3DPOOL(pVol9->Pool);
    pVol8->Size   = pVol9->Width * pVol9->Height * pVol9->Depth * GetFormatBPP(pVol8->Format);
    pVol8->Width  = pVol9->Width;
    pVol8->Height = pVol9->Height;
    pVol8->Depth  = pVol9->Depth;
  }

  // If this D3DTEXTURESTAGESTATETYPE has been remapped to a d3d9::D3DSAMPLERSTATETYPE
  // it will be returned, otherwise returns -1
  inline d3d9::D3DSAMPLERSTATETYPE GetSamplerStateType9(const D3DTEXTURESTAGESTATETYPE StageType) {
    switch (StageType) {
      // 13-21:
      case D3DTSS_ADDRESSU:       return d3d9::D3DSAMP_ADDRESSU;
      case D3DTSS_ADDRESSV:       return d3d9::D3DSAMP_ADDRESSV;
      case D3DTSS_BORDERCOLOR:    return d3d9::D3DSAMP_BORDERCOLOR;
      case D3DTSS_MAGFILTER:      return d3d9::D3DSAMP_MAGFILTER;
      case D3DTSS_MINFILTER:      return d3d9::D3DSAMP_MINFILTER;
      case D3DTSS_MIPFILTER:      return d3d9::D3DSAMP_MIPFILTER;
      case D3DTSS_MIPMAPLODBIAS:  return d3d9::D3DSAMP_MIPMAPLODBIAS;
      case D3DTSS_MAXMIPLEVEL:    return d3d9::D3DSAMP_MIPFILTER;
      case D3DTSS_MAXANISOTROPY:  return d3d9::D3DSAMP_MAXANISOTROPY;
      // 25:
      case D3DTSS_ADDRESSW:       return d3d9::D3DSAMP_ADDRESSW;
      default:                    return d3d9::D3DSAMPLERSTATETYPE(-1);
    }
  }
}

