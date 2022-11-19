#pragma once


// Implements IDirect3DSurface8

#include "d3d8_include.h"
#include "d3d8_subresource.h"
#include "d3d8_d3d9_util.h"

namespace dxvk {

  // TODO: all inherited methods in D3D8Surface should be final like in d9vk

  using D3D8SurfaceBase = D3D8Subresource<d3d9::IDirect3DSurface9, IDirect3DSurface8>;
  class D3D8Surface final : public D3D8SurfaceBase {

  public:

    D3D8Surface(
            D3D8DeviceEx*                   pDevice,
            IDirect3DBaseTexture8*          pTexture,
            Com<d3d9::IDirect3DSurface9>&&  pSurface)
      : D3D8SurfaceBase (pDevice, std::move(pSurface), pTexture) {
    }

    // A surface does not need to be attached to a texture
    D3D8Surface(
            D3D8DeviceEx*                   pDevice,
            Com<d3d9::IDirect3DSurface9>&&  pSurface)
      : D3D8Surface (pDevice, nullptr, std::move(pSurface)) {
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() {
      return D3DRESOURCETYPE(GetD3D9()->GetType());
    }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC desc;
      HRESULT res = GetD3D9()->GetDesc(&desc);
      ConvertSurfaceDesc8(&desc, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
      return GetD3D9()->LockRect((d3d9::D3DLOCKED_RECT*)pLockedRect, pRect, Flags);
    }

    HRESULT STDMETHODCALLTYPE UnlockRect() {
      return GetD3D9()->UnlockRect();
    }

    HRESULT STDMETHODCALLTYPE GetDC(HDC* phDC) {
      return GetD3D9()->GetDC(phDC);
    }

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC) {
      return GetD3D9()->ReleaseDC(hDC);
    }

  public:

    /**
     * \brief Allocate or reuse an image of the same size
     * as this texture for performing blit into system mem.
     * 
     * TODO: Consider creating only one texture to
     * encompass all surface levels of a texture.
     */
    Com<d3d9::IDirect3DSurface9> GetBlitImage() {
      if (unlikely(m_blitImage == nullptr)) {
        m_blitImage = CreateBlitImage();
      }

      return m_blitImage;
    }


  private:
    Com<d3d9::IDirect3DSurface9> CreateBlitImage();

    Com<d3d9::IDirect3DSurface9> m_blitImage = nullptr;

  };
}