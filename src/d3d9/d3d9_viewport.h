#pragma once

#include "d3d9_include.h"

namespace dxvk {
  /// Viewport-related functions implementation.
  class D3D9DeviceViewport: public virtual IDirect3DDevice9 {
  public:
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* pViewport) final override;

    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* pViewport) final override;
  };
}
