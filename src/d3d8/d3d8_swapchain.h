#pragma once

#include "d3d8_device_child.h"
#include "d3d8_surface.h"

namespace dxvk {

  using D3D8SwapChainBase = D3D8DeviceChild<d3d9::IDirect3DSwapChain9, IDirect3DSwapChain8>;
  class D3D8SwapChain final : public D3D8SwapChainBase {

  public:

    D3D8SwapChain(
            D3D8Device*                      pDevice,
            D3DPRESENT_PARAMETERS*           pPresentationParameters,
            Com<d3d9::IDirect3DSwapChain9>&& pSwapChain);

    HRESULT STDMETHODCALLTYPE Present(const RECT *src, const RECT *dst, HWND hWnd, const RGNDATA *dirtyRegion) final;

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
        UINT                BackBuffer,
        D3DBACKBUFFER_TYPE  Type,
        IDirect3DSurface8** ppBackBuffer) final;

  private:

    std::vector<Com<D3D8Surface, false>> m_backBuffers;

  };

}