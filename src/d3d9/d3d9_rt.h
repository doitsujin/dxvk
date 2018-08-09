#pragma once

#include "d3d9_surface.h"

namespace dxvk {
  /// A render target surface which stores its associated RT view.
  class D3D9RenderTarget final: public D3D9Surface {
  public:
    D3D9RenderTarget(IDirect3DDevice9* parent, ID3D11Texture2D* surface,
      Com<ID3D11RenderTargetView>&& view);

    ID3D11RenderTargetView* GetView() const {
      return m_view.ptr();
    }
  private:
    Com<ID3D11RenderTargetView> m_view;
  };
}
