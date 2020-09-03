#pragma once

/**
* Utility functions for converting
* between DirectX8 and DirectX9 types.
*/

namespace dxvk {

  // (8<-9) D3DCAPSX: Writes to D3DCAPS8 from D3DCAPS9
  inline void ConvertCaps8(const d3d9::D3DCAPS9& caps9, D3DCAPS8* pCaps8) {

    // should be aligned
    std::memcpy(pCaps8, &caps9, sizeof(D3DCAPS8));

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

    params.SwapEffect = d3d9::D3DSWAPEFFECT(pParams->SwapEffect);
    params.hDeviceWindow = pParams->hDeviceWindow;
    params.Windowed = pParams->Windowed;
    params.EnableAutoDepthStencil = pParams->EnableAutoDepthStencil;
    params.AutoDepthStencilFormat = d3d9::D3DFORMAT(pParams->AutoDepthStencilFormat);
    params.Flags = pParams->Flags;

    params.FullScreen_RefreshRateInHz = pParams->FullScreen_RefreshRateInHz;

    // FullScreen_PresentationInterval -> PresentationInterval
    params.PresentationInterval = pParams->FullScreen_PresentationInterval;

    return params;
  }

  inline UINT GetFormatBPP(const D3DFORMAT fmt) {
    // TODO: get bpp based on format
    return 32;
  }

  // (8<-9) Convert D3DSURFACE_DESC
  inline void ConvertSurfaceDesc8(const d3d9::D3DSURFACE_DESC* pSurf9, D3DSURFACE_DESC* pSurf8) {
    pSurf8->Format = D3DFORMAT(pSurf9->Format);
    pSurf8->Type = D3DRESOURCETYPE(pSurf9->Type);
    pSurf8->Usage = pSurf9->Usage;
    pSurf8->Pool = D3DPOOL(pSurf9->Pool);
    pSurf8->Size = pSurf9->Width * pSurf9->Height * GetFormatBPP(pSurf8->Format);

    pSurf8->MultiSampleType = D3DMULTISAMPLE_TYPE(pSurf9->MultiSampleType);
    // DX8: No multisample quality
    pSurf8->Width = pSurf9->Width;
    pSurf8->Height = pSurf9->Height;
  }

  // If this D3DTEXTURESTAGESTATETYPE has been remapped to a d3d9::D3DSAMPLERSTATETYPE
  // it will be returned, otherwise returns -1
  inline d3d9::D3DSAMPLERSTATETYPE GetSamplerStateType9(const D3DTEXTURESTAGESTATETYPE StageType) {
    switch (StageType) {
      // 13-21:
      case D3DTSS_ADDRESSU:       return d3d9::D3DSAMP_ADDRESSU;
      case D3DTSS_ADDRESSV:       return d3d9::D3DSAMP_ADDRESSW;
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

