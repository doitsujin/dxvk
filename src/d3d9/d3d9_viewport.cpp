#include "d3d9_device.h"

namespace dxvk {
  HRESULT D3D9Device::GetViewport(D3DVIEWPORT9* pViewport) {
    CHECK_NOT_NULL(pViewport);

    UINT num = 1;
    D3D11_VIEWPORT vp;
    m_ctx->RSGetViewports(&num, &vp);

    D3DVIEWPORT9 viewport;

    // In D3D11 it's possible for these coordinates to be floats, but since we set them
    // we know they're integers.
    viewport.X = static_cast<DWORD>(vp.TopLeftX);
    viewport.Y = static_cast<DWORD>(vp.TopLeftX);

    viewport.Width = static_cast<DWORD>(vp.Width);
    viewport.Height = static_cast<DWORD>(vp.Height);

    viewport.MinZ = vp.MinDepth;
    viewport.MaxZ = vp.MaxDepth;

    *pViewport = viewport;

    return D3D_OK;
  }

  HRESULT D3D9Device::SetViewport(const D3DVIEWPORT9* pViewport) {
    CHECK_NOT_NULL(pViewport);

    const auto& vp = *pViewport;

    D3D11_VIEWPORT newViewport;

    newViewport.TopLeftX = static_cast<float>(vp.X);
    newViewport.TopLeftY = static_cast<float>(vp.Y);

    newViewport.Width = static_cast<float>(vp.Width);
    newViewport.Height = static_cast<float>(vp.Height);

    newViewport.MinDepth = vp.MinZ;
    newViewport.MaxDepth = vp.MaxZ;

    m_ctx->RSSetViewports(1, &newViewport);

    return D3D_OK;
  }
}
