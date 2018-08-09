#pragma once

#include "d3d9_surface.h"

namespace dxvk {
  /// A surface which stores a depth / stencil buffer view.
  class D3D9DepthStencil final: public D3D9Surface {
  public:
    D3D9DepthStencil(IDirect3DDevice9* parent, ID3D11Texture2D* surface,
      Com<ID3D11DepthStencilView>&& view);

    ID3D11DepthStencilView* GetView() const {
      return m_view.ptr();
    }

  private:
    Com<ID3D11DepthStencilView> m_view;
  };
}
