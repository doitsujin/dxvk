#pragma once

#include "d3d8_device_child.h"
#include "d3d8_surface.h"
#include "d3d8_d3d9_util.h"

namespace dxvk {
  
  using D3D8SwapChainBase = D3D8DeviceChild<d3d9::IDirect3DSwapChain9, IDirect3DSwapChain8>;
  class D3D8SwapChain final : public D3D8SwapChainBase {

  public:

    D3D8SwapChain(
      D3D8DeviceEx* pDevice,
      Com<d3d9::IDirect3DSwapChain9>&& pSwapChain)
      : D3D8SwapChainBase(pDevice, std::move(pSwapChain)) {}

    HRESULT STDMETHODCALLTYPE Present(const RECT *src, const RECT *dst, HWND hWnd, const RGNDATA *dirtyRegion) final {
        return GetD3D9()->Present(src, dst, hWnd, dirtyRegion, 0);
    }
    
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface8** ppBackBuffer) final {
      // Same logic as in D3D8DeviceEx::GetBackBuffer
      if (unlikely(m_backBuffer == nullptr)) {
        Com<d3d9::IDirect3DSurface9> pSurface9;
        HRESULT res = GetD3D9()->GetBackBuffer(BackBuffer, (d3d9::D3DBACKBUFFER_TYPE)Type, &pSurface9);

        m_backBuffer = new D3D8Surface(GetParent(), std::move(pSurface9));
        *ppBackBuffer = m_backBuffer.ref();
        return res;
      }

      *ppBackBuffer = m_backBuffer.ref();
      return D3D_OK;
    }
  private:
    Com<D3D8Surface> m_backBuffer = nullptr;

  };

}