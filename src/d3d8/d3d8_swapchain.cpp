#include "d3d8_swapchain.h"

namespace dxvk {

  D3D8SwapChain::D3D8SwapChain(
          D3D8Device*                      pDevice,
          D3DPRESENT_PARAMETERS*           pPresentationParameters,
          Com<d3d9::IDirect3DSwapChain9>&& pSwapChain)
    : D3D8SwapChainBase(pDevice, std::move(pSwapChain)) {
    m_backBuffers.resize(pPresentationParameters->BackBufferCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8SwapChain::Present(const RECT *src, const RECT *dst, HWND hWnd, const RGNDATA *dirtyRegion) {
    return GetD3D9()->Present(src, dst, hWnd, dirtyRegion, 0);
  }

  HRESULT STDMETHODCALLTYPE D3D8SwapChain::GetBackBuffer(
      UINT                BackBuffer,
      D3DBACKBUFFER_TYPE  Type,
      IDirect3DSurface8** ppBackBuffer) {
    if (unlikely(ppBackBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    // Same logic as in D3D8Device::GetBackBuffer
    if (BackBuffer >= m_backBuffers.size() || m_backBuffers[BackBuffer] == nullptr) {
      Com<d3d9::IDirect3DSurface9> pSurface9;
      HRESULT res = GetD3D9()->GetBackBuffer(BackBuffer, (d3d9::D3DBACKBUFFER_TYPE)Type, &pSurface9);

      if (likely(SUCCEEDED(res))) {
        m_backBuffers[BackBuffer] = new D3D8Surface(GetParent(), D3DPOOL_DEFAULT, std::move(pSurface9));
        *ppBackBuffer = m_backBuffers[BackBuffer].ref();
      }

      return res;
    }

    *ppBackBuffer = m_backBuffers[BackBuffer].ref();
    return D3D_OK;
  }

}