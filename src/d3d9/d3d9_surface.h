#pragma once

#include "d3d9_resource.h"

namespace dxvk {
  /// A surface is a 2D array of pixels in a certain format.
  ///
  /// This could either be a pixel buffer, a depth buffer, a 2D texture,
  /// or a 2D-view of a 1D/3D/cube-map texture.
  class D3D9Surface: public ComObject<D3D9Resource<IDirect3DSurface9>> {
  public:
    D3D9Surface(IDirect3DDevice9* parent, ID3D11Texture2D* surface, DWORD usage);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final override;

    // Getters for resource info
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) final override;
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final override;
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final override;

    // Surface locking functions, which allow the CPU to access a surface directly.
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect,
      const RECT* pRect, DWORD Flags) final override;
    HRESULT STDMETHODCALLTYPE UnlockRect() final override;

    // GDI-interop functions.
    HRESULT STDMETHODCALLTYPE GetDC(HDC* pHDC) final override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC) final override;

  protected:
    Com<ID3D11Texture2D> m_surface;
    DWORD m_usage;
  };
}
