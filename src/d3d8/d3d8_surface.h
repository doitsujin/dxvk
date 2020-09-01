#pragma once


// Implements IDirect3DSurface8

#include "d3d8_include.h"
#include "d3d8_device.h"
#include "d3d8_resource.h"
#include "d3d8_texture.h"

#include "../util/util_gdi.h"

#include <algorithm>

#define D3D8_SURFACE_STUB_FINAL(...) \
(__VA_ARGS__) final { \
  Logger::warn("D3D8Surface: STUB final (" #__VA_ARGS__ ") -> HRESULT"); \
  return D3DERR_INVALIDCALL;\
}

#define D3D8_SURFACE_STUB(...) \
(__VA_ARGS__) { \
  Logger::warn("D3D8Surface: STUB (" #__VA_ARGS__ ") -> HRESULT"); \
  return D3DERR_INVALIDCALL;\
}


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

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() /*final*/ {
      // TODO: Surface::GetType
      return D3DRTYPE_TEXTURE;
    }

    HRESULT STDMETHODCALLTYPE GetContainer D3D8_SURFACE_STUB_FINAL(REFIID riid, void** ppContainer);

    HRESULT STDMETHODCALLTYPE GetDesc D3D8_SURFACE_STUB_FINAL(D3DSURFACE_DESC *pDesc);

    HRESULT STDMETHODCALLTYPE LockRect D3D8_SURFACE_STUB_FINAL(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);

    HRESULT STDMETHODCALLTYPE UnlockRect D3D8_SURFACE_STUB_FINAL();

    HRESULT STDMETHODCALLTYPE GetDC D3D8_SURFACE_STUB(HDC *phDC);

    HRESULT STDMETHODCALLTYPE ReleaseDC D3D8_SURFACE_STUB(HDC hDC);

    void ClearContainer() {}

  private:
    d3d9::IDirect3DSurface9* m_surface;
    D3D8GDIDesc m_dcDesc;

  };
}