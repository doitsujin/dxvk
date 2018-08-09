#pragma once

#include "d3d9_texture_base.h"
#include "d3d9_surface.h"

namespace dxvk {
  /// Simply a 2D texture.
  class D3D9Texture final: public ComObject<D3D9TextureBase<IDirect3DTexture9>> {
  public:
    D3D9Texture(Com<D3D9Surface>&& surface);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final override;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final override;

    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) final override;
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) final override;

    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT* pDirtyRect) final override;
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect,
      const RECT* pRect, DWORD Flags) final override;
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) final override;

  private:
    Com<D3D9Surface> m_surface;
  };
}
