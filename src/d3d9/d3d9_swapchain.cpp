#include "d3d9_swapchain.h"

#include "d3d9_monitor.h"

#include <algorithm>

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
     || (GetParent()->IsExtended() && riid == __uuidof(IDirect3DSwapChain9Ex))) {
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
    HWND window = GetPresentWindow(hDestWindowOverride);

    auto& presenter = GetOrMakePresenter(window);
    presenter.present();

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
    if (pMode == nullptr)
      return D3DERR_INVALIDCALL;

    D3DDISPLAYMODEEX mode;
    this->GetDisplayModeEx(&mode, nullptr);

    pMode->Width = mode.Width;
    pMode->Height = mode.Height;
    pMode->Format = mode.Format;
    pMode->RefreshRate = mode.RefreshRate;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DSwapChain9Ex::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    *pPresentationParameters = m_presentParams;

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
    if (pMode == nullptr && pRotation == nullptr)
      return D3DERR_INVALIDCALL;

    if (pRotation != nullptr)
      *pRotation = D3DDISPLAYROTATION_IDENTITY;

    if (pMode != nullptr) {
      ::MONITORINFOEXW monInfo;
      monInfo.cbSize = sizeof(monInfo);

      if (!::GetMonitorInfoW(GetDefaultMonitor(), reinterpret_cast<MONITORINFO*>(&monInfo))) {
        Logger::err("Direct3DSwapChain9Ex::GetDisplayModeEx: Failed to query monitor info");
        return D3DERR_INVALIDCALL;
      }

      DEVMODEW devMode = { };
      devMode.dmSize = sizeof(devMode);

      if (!::EnumDisplaySettingsW(monInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
        Logger::err("Direct3DSwapChain9Ex::GetDisplayModeEx: Failed to enum display settings");
        return D3DERR_INVALIDCALL;
      }

      pMode->Size = sizeof(D3DDISPLAYMODEEX);
      pMode->Width = devMode.dmPelsWidth;
      pMode->Height = devMode.dmPelsHeight;
      pMode->RefreshRate = devMode.dmDisplayFrequency;
      pMode->Format = D3DFMT_X8R8G8B8;
      pMode->ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
    }

    return D3D_OK;
  }

  HRESULT Direct3DSwapChain9Ex::Reset(D3DPRESENT_PARAMETERS* parameters) {
    HWND originalWindow = m_presentParams.hDeviceWindow;

    m_presentParams.hDeviceWindow = parameters->hDeviceWindow;
    
    HWND window = GetPresentWindow();
    RECT clientRect;
    GetClientRect(window, &clientRect);

    parameters->BackBufferCount = std::max(parameters->BackBufferCount, 1u);
    parameters->BackBufferWidth = parameters->BackBufferWidth ? parameters->BackBufferWidth : clientRect.right;
    parameters->BackBufferHeight = parameters->BackBufferHeight ? parameters->BackBufferHeight : clientRect.bottom;

    D3DDISPLAYMODEEX mode;
    this->GetDisplayModeEx(&mode, nullptr);
    
    D3DFORMAT format = parameters->BackBufferFormat;
    if (format == D3DFMT_UNKNOWN)
      format = mode.Format;

    parameters->BackBufferFormat = format;

    if (!parameters->Windowed)
      Logger::warn("Direct3DSwapChain9Ex::Reset: fullscreen not implemented.");

    if (originalWindow == parameters->hDeviceWindow) {
      auto& presenter = GetOrMakePresenter(window);
      presenter.recreateSwapChain(
        fixupFormat(parameters->BackBufferFormat),
        parameters->BackBufferWidth,
        parameters->BackBufferHeight,
        parameters->BackBufferCount,
        parameters->PresentationInterval != 0);
    }
    else {
      m_presenters.clear();
    }

    m_presentParams = *parameters;

    GetOrMakePresenter(window);

    return D3D_OK;
  }

  HRESULT Direct3DSwapChain9Ex::WaitForVBlank() {
    Logger::warn("Direct3DSwapChain9Ex::WaitForVBlank: Stub");
    return D3D_OK;
  }

  void    Direct3DSwapChain9Ex::SetGammaRamp(
    DWORD Flags,
    const D3DGAMMARAMP* pRamp) {
    Logger::warn("Direct3DSwapChain9Ex::SetGammaRamp: Stub");
  }

  void    Direct3DSwapChain9Ex::GetGammaRamp(D3DGAMMARAMP* pRamp) {
    Logger::warn("Direct3DSwapChain9Ex::GetGammaRamp: Stub");
  }

  D3D9Presenter& Direct3DSwapChain9Ex::GetOrMakePresenter(HWND window) {
    for (const auto& presenter : m_presenters) {
      if (presenter->window() == window)
        return *presenter;
    }

    auto* presenter = new D3D9Presenter(
      m_device,
      window,
      fixupFormat(m_presentParams.BackBufferFormat),
      m_presentParams.BackBufferWidth,
      m_presentParams.BackBufferHeight,
      m_presentParams.BackBufferCount,
      m_presentParams.PresentationInterval != 0
    );

    m_presenters.push_back(presenter);

    return *presenter;
  }

  HWND Direct3DSwapChain9Ex::GetPresentWindow(HWND windowOverride) {
    if (windowOverride != nullptr)
      return windowOverride;

    return m_presentParams.hDeviceWindow ? m_presentParams.hDeviceWindow : m_parent->GetWindow();
  }

}