#include "d3d9_swapchain.h"

#include "d3d9_monitor.h"
#include "d3d9_surface.h"

#include "../util/util_env.h"

#include <algorithm>

namespace dxvk {

  D3D9SwapChainEx::D3D9SwapChainEx(
          D3D9DeviceEx*          pDevice,
          D3DPRESENT_PARAMETERS* pPresentParams)
    : D3D9SwapChainExBase      ( pDevice )
    , m_presentParams          ( )
    , m_backBuffer             ( nullptr )
    , m_gammaFlags             ( 0 ) {
    SetDefaultGamma();
    Reset(pPresentParams, true);
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DSwapChain9)
     || (GetParent()->IsExtended() && riid == __uuidof(IDirect3DSwapChain9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9SwapChainEx::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect,
    HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
    DWORD dwFlags) {
    auto lock = m_parent->LockDevice();

    HWND window = GetPresentWindow(&m_presentParams, hDestWindowOverride);

    auto& presenter = GetOrMakePresenter(window);

    m_parent->Flush();
    m_parent->SynchronizeCsThread();

    presenter.present();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetFrontBufferData(IDirect3DSurface9* pDestSurface) {
    Logger::warn("D3D9SwapChainEx::GetFrontBufferData: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetBackBuffer(
          UINT                iBackBuffer,
          D3DBACKBUFFER_TYPE  Type,
          IDirect3DSurface9** ppBackBuffer) {
    InitReturnPtr(ppBackBuffer);

    if (ppBackBuffer == nullptr || iBackBuffer != 0)
      return D3DERR_INVALIDCALL;

    *ppBackBuffer = ref(m_backBuffer);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) {
    Logger::warn("D3D9SwapChainEx::GetRasterStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetDisplayMode(D3DDISPLAYMODE* pMode) {
    if (pMode == nullptr)
      return D3DERR_INVALIDCALL;

    *pMode = D3DDISPLAYMODE();

    D3DDISPLAYMODEEX mode;
    HRESULT hr = this->GetDisplayModeEx(&mode, nullptr);

    if (FAILED(hr))
      return hr;

    pMode->Width = mode.Width;
    pMode->Height = mode.Height;
    pMode->Format = mode.Format;
    pMode->RefreshRate = mode.RefreshRate;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    *pPresentationParameters = m_presentParams;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetLastPresentCount(UINT* pLastPresentCount) {
    Logger::warn("D3D9SwapChainEx::GetLastPresentCount: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetPresentStats(D3DPRESENTSTATS* pPresentationStatistics) {
    Logger::warn("D3D9SwapChainEx::GetPresentStats: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetDisplayModeEx(D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation) {
    if (pMode == nullptr && pRotation == nullptr)
      return D3DERR_INVALIDCALL;

    if (pRotation != nullptr)
      *pRotation = D3DDISPLAYROTATION_IDENTITY;

    if (pMode != nullptr) {
      ::MONITORINFOEXW monInfo;
      monInfo.cbSize = sizeof(monInfo);

      if (!::GetMonitorInfoW(GetDefaultMonitor(), reinterpret_cast<MONITORINFO*>(&monInfo))) {
        Logger::err("D3D9SwapChainEx::GetDisplayModeEx: Failed to query monitor info");
        return D3DERR_INVALIDCALL;
      }

      DEVMODEW devMode = { };
      devMode.dmSize = sizeof(devMode);

      if (!::EnumDisplaySettingsW(monInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
        Logger::err("D3D9SwapChainEx::GetDisplayModeEx: Failed to enum display settings");
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

  HRESULT D3D9SwapChainEx::Reset(D3DPRESENT_PARAMETERS* parameters, bool first) {
    HWND newWindow      = GetPresentWindow(parameters);
    HWND originalWindow = GetPresentWindow(&m_presentParams);

    RECT clientRect;
    GetClientRect(newWindow, &clientRect);

    parameters->BackBufferCount  = std::max(parameters->BackBufferCount, 1u);
    parameters->BackBufferWidth  = parameters->BackBufferWidth  ? parameters->BackBufferWidth  : clientRect.right;
    parameters->BackBufferHeight = parameters->BackBufferHeight ? parameters->BackBufferHeight : clientRect.bottom;

    D3DDISPLAYMODEEX mode;
    HRESULT hr = this->GetDisplayModeEx(&mode, nullptr);
    if (FAILED(hr))
      return hr;

    D3DFORMAT format = parameters->BackBufferFormat;
    if (format == D3DFMT_UNKNOWN)
      format = mode.Format;

    parameters->BackBufferFormat = format;

    if (env::getEnvVar("DXVK_FORCE_WINDOWED") == "1")
      parameters->Windowed = TRUE;

    if (!parameters->Windowed && (m_presentParams.Windowed || first)) {
      RECT windowRect = { 0, 0, 0, 0 };
      ::GetWindowRect(newWindow, &windowRect);

      HMONITOR monitor = ::MonitorFromPoint(
        { (windowRect.left + windowRect.right) / 2,
          (windowRect.top + windowRect.bottom) / 2 },
        MONITOR_DEFAULTTOPRIMARY);

      // TODO: change display mode via win32

      // Change the window flags to remove the decoration etc.
      LONG style   = ::GetWindowLongW(newWindow, GWL_STYLE);
      LONG exstyle = ::GetWindowLongW(newWindow, GWL_EXSTYLE);

      m_windowState.style = style;
      m_windowState.exstyle = exstyle;

      style   &= ~WS_OVERLAPPEDWINDOW;
      exstyle &= ~WS_EX_OVERLAPPEDWINDOW;

      ::SetWindowLongW(newWindow, GWL_STYLE, style);
      ::SetWindowLongW(newWindow, GWL_EXSTYLE, exstyle);

      // Move the window so that it covers the entire output
      ::MONITORINFO monInfo;
      monInfo.cbSize = sizeof(monInfo);

      if (!::GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
        Logger::err("D3D9: Failed to query monitor info");
        return E_FAIL;
      }

      const RECT rect = monInfo.rcMonitor;

      ::SetWindowPos(newWindow, HWND_TOPMOST,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);

    } else if (parameters->Windowed && !m_presentParams.Windowed && !first) {
      LONG curStyle   = ::GetWindowLongW(originalWindow, GWL_STYLE) & ~WS_VISIBLE;
      LONG curExstyle = ::GetWindowLongW(originalWindow, GWL_EXSTYLE) & ~WS_EX_TOPMOST;

      if (curStyle == (m_windowState.style & ~(WS_VISIBLE | WS_OVERLAPPEDWINDOW))
       && curExstyle == (m_windowState.exstyle & ~(WS_EX_TOPMOST | WS_EX_OVERLAPPEDWINDOW))) {
        ::SetWindowLongW(originalWindow, GWL_STYLE,   m_windowState.style);
        ::SetWindowLongW(originalWindow, GWL_EXSTYLE, m_windowState.exstyle);
      }

      // Restore window position and apply the style
      const RECT rect = m_windowState.rect;

      ::SetWindowPos(originalWindow, 0,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    m_presentParams = *parameters;

    if (!m_presenters.empty() && newWindow == originalWindow) {
      auto& presenter = GetOrMakePresenter(newWindow);

      D3D9PresenterDesc desc = CalcPresenterDesc();
      presenter.recreateSwapChain(&desc);
      presenter.createBackBuffer();
    }
    else {
      m_presenters.clear();
    }

    auto& presenter = GetOrMakePresenter(newWindow);

    if (m_backBuffer != nullptr)
      m_backBuffer->ReleasePrivate();

    m_backBuffer = new D3D9Surface(
      m_parent,
      presenter.getBackBuffer(),
      0, 0,
      this );

    m_backBuffer->AddRefPrivate();

    return D3D_OK;
  }

  HRESULT D3D9SwapChainEx::WaitForVBlank() {
    Logger::warn("D3D9SwapChainEx::WaitForVBlank: Stub");
    return D3D_OK;
  }

  void D3D9SwapChainEx::SetDefaultGamma() {
    for (uint32_t i = 0; i < 256; i++) {
      m_gammaRamp.red[i]   = i * 257;
      m_gammaRamp.green[i] = i * 257;
      m_gammaRamp.blue[i]  = i * 257;
    }
  }

  void    D3D9SwapChainEx::SetGammaRamp(
          DWORD         Flags,
    const D3DGAMMARAMP* pRamp) {
    m_gammaFlags = Flags;

    if (pRamp != nullptr)
      m_gammaRamp = *pRamp;
    else
      SetDefaultGamma();

    auto& presenter = GetOrMakePresenter(GetPresentWindow(&m_presentParams));

    presenter.setGammaRamp(Flags, &m_gammaRamp);
  }

  void    D3D9SwapChainEx::GetGammaRamp(D3DGAMMARAMP* pRamp) {
    if (pRamp != nullptr)
      *pRamp = m_gammaRamp;
  }

  D3D9PresenterDesc D3D9SwapChainEx::CalcPresenterDesc() {
    auto options = m_parent->GetOptions();

    D3D9PresenterDesc desc;
    desc.bufferCount     = m_presentParams.BackBufferCount;
    desc.width           = m_presentParams.BackBufferWidth;
    desc.height          = m_presentParams.BackBufferHeight;
    desc.format          = EnumerateFormat(m_presentParams.BackBufferFormat);
    desc.presentInterval = m_presentParams.PresentationInterval;
    desc.multisample     = m_presentParams.MultiSampleType;

    if (desc.presentInterval == D3DPRESENT_INTERVAL_IMMEDIATE)
      desc.presentInterval = 0;

    if (options->presentInterval >= 0)
      desc.presentInterval = options->presentInterval;

    return desc;
  }

  D3D9Presenter& D3D9SwapChainEx::GetOrMakePresenter(HWND window) {
    for (const auto& presenter : m_presenters) {
      if (presenter->window() == window)
        return *presenter;
    }

    D3D9PresenterDesc desc = CalcPresenterDesc();

    auto* presenter = new D3D9Presenter(
      m_parent,
      window,
      &desc,
      m_gammaFlags,
      &m_gammaRamp);

    m_presenters.push_back(presenter);

    return *presenter;
  }

  HWND D3D9SwapChainEx::GetPresentWindow(D3DPRESENT_PARAMETERS* parameters, HWND windowOverride) {
    if (windowOverride != nullptr)
      return windowOverride;

    return parameters->hDeviceWindow ? parameters->hDeviceWindow : m_parent->GetWindow();
  }

}
