#pragma once

#include "d3d9_include.h"

namespace dxvk {
  /// Implementation of cursor-related device functions.
  class D3D9DeviceCursor: public virtual IDirect3DDevice9 {
  public:
    void STDMETHODCALLTYPE SetCursorPosition(int X, int Y, DWORD Flags) final override;

    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) final override;

    BOOL STDMETHODCALLTYPE ShowCursor(BOOL bShow) final override;
  };
}
