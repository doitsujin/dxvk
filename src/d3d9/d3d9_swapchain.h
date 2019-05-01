#pragma once

#include "d3d9_device_child.h"
#include "d3d9_format.h"
#include "d3d9_presenter.h"

#include <vector>

namespace dxvk {

  class D3D9Surface;

  using D3D9SwapChainExBase = D3D9DeviceChild<IDirect3DSwapChain9Ex>;
  class D3D9SwapChainEx final : public D3D9SwapChainExBase {

  public:

    D3D9SwapChainEx(
            D3D9DeviceEx*          pDevice,
            D3DPRESENT_PARAMETERS* pPresentParams);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Present(
      const RECT* pSourceRect,
      const RECT* pDestRect,
      HWND hDestWindowOverride,
      const RGNDATA* pDirtyRegion,
      DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSurface9* pDestSurface);

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
      UINT iBackBuffer,
      D3DBACKBUFFER_TYPE Type,
      IDirect3DSurface9** ppBackBuffer);

    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters);

    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount);

    HRESULT STDMETHODCALLTYPE GetPresentStats(D3DPRESENTSTATS* pPresentationStatistics);

    HRESULT STDMETHODCALLTYPE GetDisplayModeEx(D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation);

    HRESULT Reset(D3DPRESENT_PARAMETERS* parameters, bool = false);

    HRESULT WaitForVBlank();

    void SetDefaultGamma();

    void    SetGammaRamp(
            DWORD         Flags,
      const D3DGAMMARAMP* pRamp);

    void    GetGammaRamp(D3DGAMMARAMP* pRamp);

    HWND GetPresentWindow(D3DPRESENT_PARAMETERS* parameters, HWND windowOverride = nullptr);

  private:

    struct WindowState {
    	LONG style = 0;
    	LONG exstyle = 0;
    	RECT rect = { 0, 0, 0, 0 };
    };

    D3D9PresenterDesc CalcPresenterDesc();

    D3D9Presenter& GetOrMakePresenter(HWND window);

    Rc<DxvkDevice> m_device;

    std::vector<Rc<D3D9Presenter>> m_presenters;

    D3DPRESENT_PARAMETERS m_presentParams;
    WindowState m_windowState;
    
    D3D9Surface* m_backBuffer;

    DWORD m_gammaFlags;
    D3DGAMMARAMP m_gammaRamp;

  };

}