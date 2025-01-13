#pragma once

#include "d3d8_include.h"
#include "d3d8_subresource.h"

namespace dxvk {

  // TODO: all inherited methods in D3D8Surface should be final like in d9vk

  using D3D8SurfaceBase = D3D8Subresource<d3d9::IDirect3DSurface9, IDirect3DSurface8>;
  class D3D8Surface final : public D3D8SurfaceBase {

  public:

    D3D8Surface(
            D3D8Device*                     pDevice,
            IDirect3DBaseTexture8*          pTexture,
            Com<d3d9::IDirect3DSurface9>&&  pSurface);

    D3D8Surface(
            D3D8Device*                     pDevice,
            Com<d3d9::IDirect3DSurface9>&&  pSurface);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType();

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE LockRect(
        D3DLOCKED_RECT* pLockedRect,
        CONST RECT*     pRect,
        DWORD           Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect();

    HRESULT STDMETHODCALLTYPE GetDC(HDC* phDC);

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC);

    /**
     * \brief Allocate or reuse an image of the same size
     * as this texture for performing blit into system mem.
     */
    Com<d3d9::IDirect3DSurface9> GetBlitImage();

  private:

    Com<d3d9::IDirect3DSurface9> CreateBlitImage();

    Com<d3d9::IDirect3DSurface9> m_blitImage;

  };

}