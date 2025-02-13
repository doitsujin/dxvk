#pragma once

#include "d3d8_include.h"
#include "d3d8_subresource.h"

namespace dxvk {

  // Note: IDirect3DSurface8 does not actually inherit from IDirect3DResource8,
  // however it does expose serveral of the methods typically found part of
  // IDirect3DResource8, such as Set/Get/FreePrivateData, so model it as such.
  using D3D8SurfaceBase = D3D8Subresource<d3d9::IDirect3DSurface9, IDirect3DSurface8>;
  class D3D8Surface final : public D3D8SurfaceBase {

  public:

    D3D8Surface(
            D3D8Device*                     pDevice,
      const D3DPOOL                         Pool,
            IDirect3DBaseTexture8*          pTexture,
            Com<d3d9::IDirect3DSurface9>&&  pSurface);

    D3D8Surface(
            D3D8Device*                     pDevice,
      const D3DPOOL                         Pool,
            Com<d3d9::IDirect3DSurface9>&&  pSurface);

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) final;

    HRESULT STDMETHODCALLTYPE LockRect(
        D3DLOCKED_RECT* pLockedRect,
        CONST RECT*     pRect,
        DWORD           Flags) final;

    HRESULT STDMETHODCALLTYPE UnlockRect() final;

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