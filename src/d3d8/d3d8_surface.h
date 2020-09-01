#pragma once


// Implements IDirect3DSurface8

#include "d3d8_include.h"
#include "d3d8_device.h"
#include "d3d8_resource.h"
#include "d3d8_texture.h"
#include "d3d8_d3d9_util.h"

#include "../util/util_gdi.h"

#include <algorithm>

namespace dxvk {

  struct D3D8_COMMON_TEXTURE_DESC;

  using D3D8GDIDesc = D3DKMT_DESTROYDCFROMMEMORY;

  // TODO: all inherited methods in D3D8Surface should be final like in d9vk

  using D3D8SurfaceBase = D3D8Resource<IDirect3DSurface8>;
  class D3D8Surface final : public D3D8SurfaceBase {

  public:

    D3D8Surface(
            D3D8DeviceEx*             pDevice,
            d3d9::IDirect3DSurface9*  pSurface)
      : D3D8SurfaceBase (pDevice)
      , m_surface       (pSurface) { }

    D3D8Surface(
            D3D8DeviceEx*             pDevice,
            d3d9::IDirect3DSurface9*  pSurface,
            UINT                      Face,
            UINT                      MipLevel,
            IDirect3DBaseTexture8*    pBaseTexture)
      : D3D8SurfaceBase (pDevice)
      , m_surface       (pSurface) { }

    void AddRefPrivate() {}

    void ReleasePrivate() {}

    // TODO: Surface::QueryInterface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() {
      return (D3DRESOURCETYPE)m_surface->GetType();
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) {
      return m_surface->GetContainer(riid, ppContainer);
    }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) {
      d3d9::D3DSURFACE_DESC desc;
      HRESULT res = m_surface->GetDesc(&desc);
      ConvertSurfaceDesc8(&desc, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
      return m_surface->LockRect((d3d9::D3DLOCKED_RECT*)pLockedRect, pRect, Flags);
    }

    HRESULT STDMETHODCALLTYPE UnlockRect() {
      return m_surface->UnlockRect();
    }

    HRESULT STDMETHODCALLTYPE GetDC(HDC* phDC) {
      return m_surface->GetDC(phDC);
    }

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC) {
      return m_surface->ReleaseDC(hDC);
    }

  private:
    d3d9::IDirect3DSurface9* m_surface;
    D3D8GDIDesc m_dcDesc;

  };
}