#include "d3d9_device_viewport.h"

namespace dxvk {
  HRESULT D3D9DeviceViewport::GetViewport(D3DVIEWPORT9* pViewport) {
    CHECK_NOT_NULL(pViewport);

    UINT num = 1;
    D3D11_VIEWPORT vp;
    m_ctx->RSGetViewports(&num, &vp);

    *pViewport = D3DVIEWPORT9 {
      // D3D11 allows these to be floats, but since we set the viewport,
      // they will always be integers.
      (DWORD)vp.TopLeftX, (DWORD)vp.TopLeftY,
      (DWORD)vp.Width, (DWORD)vp.Height,
      vp.MinDepth, vp.MaxDepth
    };

    return D3D_OK;
  }

  HRESULT D3D9DeviceViewport::SetViewport(const D3DVIEWPORT9* pViewport) {
    CHECK_NOT_NULL(pViewport);

    const auto& vp = *pViewport;

    const D3D11_VIEWPORT newViewport {
      (FLOAT)vp.X, (FLOAT)vp.Y,
      (FLOAT)vp.Width, (FLOAT)vp.Height,
      vp.MinZ, vp.MaxZ
    };

    m_ctx->RSSetViewports(1, &newViewport);

    return D3D_OK;
  }
}
