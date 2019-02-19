#pragma once

#include "d3d9_subresource.h"

namespace dxvk {

  class Direct3DSurface9 final : public Direct3DSubresource9<IDirect3DSurface9> {

  public:

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *pDesc) final;

    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) final;

    HRESULT STDMETHODCALLTYPE UnlockRect() final;

    HRESULT STDMETHODCALLTYPE GetDC(HDC *phdc) final;

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hdc) final;

  };
}