#include "d3d9_swapchain.h"

namespace dxvk {

  Direct3DSwapChain9Ex::Direct3DSwapChain9Ex(Direct3DDevice9Ex* device, D3DPRESENT_PARAMETERS* presentParams)
    : Direct3DSwapChain9ExBase{ device } {
    Reset(presentParams);
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DSwapChain9)
     || (GetD3D9Device()->IsExtended() && riid == __uuidof(IDirect3DSwapChain9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DSwapChain9Ex::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect,
    HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
    DWORD dwFlags) {
    Logger::warn("Direct3DSwapChain9Ex::Present: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetFrontBufferData(IDirect3DSurface9* pDestSurface) {
    Logger::warn("Direct3DSwapChain9Ex::GetFrontBufferData: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetBackBuffer(
    UINT iBackBuffer,
    D3DBACKBUFFER_TYPE Type,
    IDirect3DSurface9** ppBackBuffer) {
    Logger::warn("Direct3DSwapChain9Ex::GetBackBuffer: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) {
    Logger::warn("Direct3DSwapChain9Ex::GetRasterStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetDisplayMode(D3DDISPLAYMODE* pMode) {
    Logger::warn("Direct3DSwapChain9Ex::GetDisplayMode: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    Logger::warn("Direct3DSwapChain9Ex::GetPresentParameters: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetLastPresentCount(UINT* pLastPresentCount) {
    Logger::warn("Direct3DSwapChain9Ex::GetLastPresentCount: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetPresentStats(D3DPRESENTSTATS* pPresentationStatistics) {
    Logger::warn("Direct3DSwapChain9Ex::GetPresentStats: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetDisplayModeEx(D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation) {
    Logger::warn("Direct3DSwapChain9Ex::GetDisplayModeEx: Stub");
    return D3D_OK;
  }

  HRESULT Direct3DSwapChain9Ex::Reset(D3DPRESENT_PARAMETERS* parameters) {
    Logger::warn("Direct3DSwapChain9Ex::Reset: Stub");
    return D3D_OK;
  }

  HRESULT Direct3DSwapChain9Ex::WaitForVBlank() {
    Logger::warn("Direct3DSwapChain9Ex::WaitForVBlank: Stub");
    return D3D_OK;
  }

}