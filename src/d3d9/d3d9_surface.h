#pragma once

#include "d3d9_resource.h"

namespace dxvk {
  class D3D9Surface final: public ComObject<D3D9Resource<IDirect3DSurface9>> {
  public:
    D3D9Surface(IDirect3DDevice9* parent, ID3D11Texture2D* surface, DWORD usage);

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) final override;

    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect,
      const RECT* pRect, DWORD Flags) final override;
    HRESULT STDMETHODCALLTYPE UnlockRect() final override;

    HRESULT STDMETHODCALLTYPE GetDC(HDC* pHDC) final override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC) final override;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final override {
      return D3DRTYPE_SURFACE;
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final override {
      InitReturnPtr(ppContainer);
      CHECK_NOT_NULL(ppContainer);

      Logger::err("D3D9Surface::GetContainer not implemented");
      return D3DERR_INVALIDCALL;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final override;
  protected:
    Com<ID3D11Texture2D> m_surface;
    DWORD m_usage;
  };
}
