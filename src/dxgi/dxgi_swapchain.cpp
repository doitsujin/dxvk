#include "dxgi_factory.h"
#include "dxgi_output.h"
#include "dxgi_swapchain.h"

namespace dxvk {
  
  DxgiSwapChain::DxgiSwapChain(
          IDXGIFactory*               pFactory,
          IDXGIVkSwapChain*           pPresenter,
          HWND                        hWnd,
    const DXGI_SWAP_CHAIN_DESC1*      pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*  pFullscreenDesc)
  : m_factory   (pFactory),
    m_window    (hWnd),
    m_desc      (*pDesc),
    m_descFs    (*pFullscreenDesc),
    m_presentCount(0u),
    m_presenter (pPresenter),
    m_monitor   (nullptr) {
    if (FAILED(m_presenter->GetAdapter(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&m_adapter))))
      throw DxvkError("DXGI: Failed to get adapter for present device");
    
    // Query monitor info form DXVK's DXGI factory, if available
    m_factory->QueryInterface(__uuidof(IDXGIVkMonitorInfo), reinterpret_cast<void**>(&m_monitorInfo));
    
    // Apply initial window mode and fullscreen state
    if (!m_descFs.Windowed && FAILED(EnterFullscreenMode(nullptr)))
      throw DxvkError("DXGI: Failed to set initial fullscreen state");
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    RestoreDisplayMode(m_monitor);

    // Decouple swap chain from monitor if necessary
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;
    
    if (SUCCEEDED(AcquireMonitorData(m_monitor, &monitorInfo))) {
      if (monitorInfo->pSwapChain == this)
        monitorInfo->pSwapChain = nullptr;
      
      ReleaseMonitorData();
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDeviceSubObject)
     || riid == __uuidof(IDXGISwapChain)
     || riid == __uuidof(IDXGISwapChain1)
     || riid == __uuidof(IDXGISwapChain2)
     || riid == __uuidof(IDXGISwapChain3)
     || riid == __uuidof(IDXGISwapChain4)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("DxgiSwapChain::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetParent(REFIID riid, void** ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDevice(REFIID riid, void** ppDevice) {
    return m_presenter->GetDevice(riid, ppDevice);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    return m_presenter->GetImage(Buffer, riid, ppSurface);
  }


  UINT STDMETHODCALLTYPE DxgiSwapChain::GetCurrentBackBufferIndex() {
    return m_presenter->GetImageIndex();
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    InitReturnPtr(ppOutput);
    
    if (!IsWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;
    
    if (m_target != nullptr) {
      *ppOutput = m_target.ref();
      return S_OK;
    }

    RECT windowRect = { 0, 0, 0, 0 };
    ::GetWindowRect(m_window, &windowRect);
    
    HMONITOR monitor = ::MonitorFromPoint(
      { (windowRect.left + windowRect.right) / 2,
        (windowRect.top + windowRect.bottom) / 2 },
      MONITOR_DEFAULTTOPRIMARY);
    
    return GetOutputFromMonitor(monitor, ppOutput);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    if (!pDesc)
      return E_INVALIDARG;
    
    pDesc->BufferDesc.Width     = m_desc.Width;
    pDesc->BufferDesc.Height    = m_desc.Height;
    pDesc->BufferDesc.RefreshRate = m_descFs.RefreshRate;
    pDesc->BufferDesc.Format    = m_desc.Format;
    pDesc->BufferDesc.ScanlineOrdering = m_descFs.ScanlineOrdering;
    pDesc->BufferDesc.Scaling   = m_descFs.Scaling;
    pDesc->SampleDesc           = m_desc.SampleDesc;
    pDesc->BufferUsage          = m_desc.BufferUsage;
    pDesc->BufferCount          = m_desc.BufferCount;
    pDesc->OutputWindow         = m_window;
    pDesc->Windowed             = m_descFs.Windowed;
    pDesc->SwapEffect           = m_desc.SwapEffect;
    pDesc->Flags                = m_desc.Flags;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) {
    if (pDesc == nullptr)
      return E_INVALIDARG;
    
    *pDesc = m_desc;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetBackgroundColor(
          DXGI_RGBA*                pColor) {
    Logger::err("DxgiSwapChain::GetBackgroundColor: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetRotation(
          DXGI_MODE_ROTATION*       pRotation) {
    Logger::err("DxgiSwapChain::GetRotation: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetRestrictToOutput(
          IDXGIOutput**             ppRestrictToOutput) {
    InitReturnPtr(ppRestrictToOutput);
    
    Logger::err("DxgiSwapChain::GetRestrictToOutput: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);

    if (!pStats)
      return E_INVALIDARG;

    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("DxgiSwapChain::GetFrameStatistics: Semi-stub");

    // TODO deal with the refresh counts at some point
    pStats->PresentCount = m_presentCount;
    pStats->PresentRefreshCount = 0;
    pStats->SyncRefreshCount = 0;
    QueryPerformanceCounter(&pStats->SyncQPCTime);
    pStats->SyncGPUTime.QuadPart = 0;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    HRESULT hr = S_OK;
    
    if (pFullscreen != nullptr)
      *pFullscreen = !m_descFs.Windowed;
    
    if (ppTarget != nullptr)
      *ppTarget = m_target.ref();

    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenDesc(
          DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) {
    if (pDesc == nullptr)
      return E_INVALIDARG;
    
    *pDesc = m_descFs;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetHwnd(
          HWND*                     pHwnd) {
    if (pHwnd == nullptr)
      return E_INVALIDARG;
    
    *pHwnd = m_window;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetCoreWindow(
          REFIID                    refiid,
          void**                    ppUnk) {
    InitReturnPtr(ppUnk);
    
    Logger::err("DxgiSwapChain::GetCoreWindow: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    if (pLastPresentCount == nullptr)
      return E_INVALIDARG;
    
    *pLastPresentCount = m_presentCount;
    return S_OK;
  }
  
  
  BOOL STDMETHODCALLTYPE DxgiSwapChain::IsTemporaryMonoSupported() {
    // This seems to be related to stereo 3D display
    // modes, which we don't support at the moment
    return FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    return Present1(SyncInterval, Flags, nullptr);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::Present1(
          UINT                      SyncInterval,
          UINT                      PresentFlags,
    const DXGI_PRESENT_PARAMETERS*  pPresentParameters) {
    if (!IsWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;
    
    if (SyncInterval > 4)
      return DXGI_ERROR_INVALID_CALL;

    std::lock_guard<dxvk::recursive_mutex> lockWin(m_lockWindow);
    std::lock_guard<dxvk::mutex> lockBuf(m_lockBuffer);

    try {
      HRESULT hr = m_presenter->Present(SyncInterval, PresentFlags, nullptr);
      if (hr == S_OK && !(PresentFlags & DXGI_PRESENT_TEST))
        m_presentCount++;
      return hr;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeBuffers(
          UINT        BufferCount,
          UINT        Width,
          UINT        Height,
          DXGI_FORMAT NewFormat,
          UINT        SwapChainFlags) {
    if (!IsWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;

    constexpr UINT PreserveFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    if ((m_desc.Flags & PreserveFlags) != (SwapChainFlags & PreserveFlags))
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<dxvk::mutex> lock(m_lockBuffer);
    m_desc.Width  = Width;
    m_desc.Height = Height;
    
    GetWindowClientSize(m_window,
      m_desc.Width  ? nullptr : &m_desc.Width,
      m_desc.Height ? nullptr : &m_desc.Height);
    
    if (BufferCount != 0)
      m_desc.BufferCount = BufferCount;
    
    if (NewFormat != DXGI_FORMAT_UNKNOWN)
      m_desc.Format = NewFormat;
    
    return m_presenter->ChangeProperties(&m_desc);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeBuffers1(
          UINT                      BufferCount,
          UINT                      Width,
          UINT                      Height,
          DXGI_FORMAT               Format,
          UINT                      SwapChainFlags,
    const UINT*                     pCreationNodeMask,
          IUnknown* const*          ppPresentQueue) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("DxgiSwapChain::ResizeBuffers1: Stub");

    return ResizeBuffers(BufferCount,
      Width, Height, Format, SwapChainFlags);
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);

    if (pNewTargetParameters == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    if (!IsWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;

    // Update the swap chain description
    if (pNewTargetParameters->RefreshRate.Numerator != 0)
      m_descFs.RefreshRate = pNewTargetParameters->RefreshRate;
    
    m_descFs.ScanlineOrdering = pNewTargetParameters->ScanlineOrdering;
    m_descFs.Scaling          = pNewTargetParameters->Scaling;
    
    if (m_descFs.Windowed) {
      // Adjust window position and size
      RECT newRect = { 0, 0, 0, 0 };
      RECT oldRect = { 0, 0, 0, 0 };
      
      ::GetWindowRect(m_window, &oldRect);
      ::SetRect(&newRect, 0, 0, pNewTargetParameters->Width, pNewTargetParameters->Height);
      ::AdjustWindowRectEx(&newRect,
        ::GetWindowLongW(m_window, GWL_STYLE), FALSE,
        ::GetWindowLongW(m_window, GWL_EXSTYLE));
      ::SetRect(&newRect, 0, 0, newRect.right - newRect.left, newRect.bottom - newRect.top);
      ::OffsetRect(&newRect, oldRect.left, oldRect.top);    
      ::MoveWindow(m_window, newRect.left, newRect.top,
          newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
    } else {
      Com<IDXGIOutput> output;
      
      if (FAILED(GetOutputFromMonitor(m_monitor, &output))) {
        Logger::err("DXGI: ResizeTarget: Failed to query containing output");
        return E_FAIL;
      }
      
      // If the swap chain allows it, change the display mode
      if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH) {
        ChangeDisplayMode(output.ptr(), pNewTargetParameters);
        NotifyModeChange(m_monitor, FALSE);
      }
      
      // Resize and reposition the window to 
      DXGI_OUTPUT_DESC desc;
      output->GetDesc(&desc);
      
      RECT newRect = desc.DesktopCoordinates;
      
      ::MoveWindow(m_window, newRect.left, newRect.top,
          newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
    }
    
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);

    if (!Fullscreen && pTarget)
      return DXGI_ERROR_INVALID_CALL;

    if (m_descFs.Windowed && Fullscreen)
      return this->EnterFullscreenMode(pTarget);
    else if (!m_descFs.Windowed && !Fullscreen)
      return this->LeaveFullscreenMode();
    
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetBackgroundColor(
    const DXGI_RGBA*                pColor) {
    Logger::err("DxgiSwapChain::SetBackgroundColor: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetRotation(
          DXGI_MODE_ROTATION        Rotation) {
    Logger::err("DxgiSwapChain::SetRotation: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HANDLE STDMETHODCALLTYPE DxgiSwapChain::GetFrameLatencyWaitableObject() {
    if (!(m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT))
      return nullptr;

    return m_presenter->GetFrameLatencyEvent();
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetMatrixTransform(
          DXGI_MATRIX_3X2_F*        pMatrix) {
    // We don't support composition swap chains
    Logger::err("DxgiSwapChain::GetMatrixTransform: Not supported");
    return DXGI_ERROR_INVALID_CALL;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetMaximumFrameLatency(
          UINT*                     pMaxLatency) {
    if (!(m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT))
      return DXGI_ERROR_INVALID_CALL;

    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);
    *pMaxLatency = m_presenter->GetFrameLatency();
    return S_OK;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetSourceSize(
          UINT*                     pWidth,
          UINT*                     pHeight) {
    // TODO implement properly once supported
    if (pWidth)  *pWidth  = m_desc.Width;
    if (pHeight) *pHeight = m_desc.Height;
    return S_OK;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetMatrixTransform(
    const DXGI_MATRIX_3X2_F*        pMatrix) {
    // We don't support composition swap chains
    Logger::err("DxgiSwapChain::SetMatrixTransform: Not supported");
    return DXGI_ERROR_INVALID_CALL;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetMaximumFrameLatency(
          UINT                      MaxLatency) {
    if (!(m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT))
      return DXGI_ERROR_INVALID_CALL;

    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);
    return m_presenter->SetFrameLatency(MaxLatency);
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetSourceSize(
          UINT                      Width,
          UINT                      Height) {
    if (Width  == 0 || Width  > m_desc.Width
     || Height == 0 || Height > m_desc.Height)
      return E_INVALIDARG;

    RECT region;
    region.left   = 0;
    region.top    = 0;
    region.right  = Width;
    region.bottom = Height;
    return m_presenter->SetPresentRegion(&region);
  }
  

  HRESULT STDMETHODCALLTYPE DxgiSwapChain::CheckColorSpaceSupport(
          DXGI_COLOR_SPACE_TYPE           ColorSpace,
          UINT*                           pColorSpaceSupport) {
    if (!pColorSpaceSupport)
      return E_INVALIDARG;
    
    UINT supportFlags = 0;

    if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
      supportFlags |= DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT;

    *pColorSpaceSupport = supportFlags;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) {
    UINT support = 0;

    HRESULT hr = CheckColorSpaceSupport(ColorSpace, &support);

    if (FAILED(hr))
      return hr;

    if (!support)
      return E_INVALIDARG;

    return S_OK;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetHDRMetaData(
          DXGI_HDR_METADATA_TYPE    Type,
          UINT                      Size,
          void*                     pMetaData) {
    if (Size && !pMetaData)
      return E_INVALIDARG;

    switch (Type) {
      case DXGI_HDR_METADATA_TYPE_NONE:
        return S_OK;

      case DXGI_HDR_METADATA_TYPE_HDR10:
        if (Size != sizeof(DXGI_HDR_METADATA_HDR10))
          return E_INVALIDARG;

        // For some reason this always seems to succeed on Windows
        Logger::warn("DXGI: HDR not supported");
        return S_OK;

      default:
        Logger::err(str::format("DXGI: Invalid HDR metadata type: ", Type));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetGammaControl(
          UINT                      NumPoints,
    const DXGI_RGB*                 pGammaCurve) {
    std::lock_guard<dxvk::mutex> lockBuf(m_lockBuffer);
    return m_presenter->SetGammaControl(NumPoints, pGammaCurve);
  }


  HRESULT DxgiSwapChain::EnterFullscreenMode(IDXGIOutput* pTarget) {
    Com<IDXGIOutput> output = pTarget;

    if (!IsWindow(m_window))
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    
    if (output == nullptr) {
      if (FAILED(GetContainingOutput(&output))) {
        Logger::err("DXGI: EnterFullscreenMode: Cannot query containing output");
        return E_FAIL;
      }
    }
    
    // Find a display mode that matches what we need
    ::GetWindowRect(m_window, &m_windowState.rect);
    
    if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH) {
      DXGI_MODE_DESC displayMode;
      displayMode.Width            = m_desc.Width;
      displayMode.Height           = m_desc.Height;
      displayMode.RefreshRate      = m_descFs.RefreshRate;
      displayMode.Format           = m_desc.Format;
      // Ignore these two, games usually use them wrong and we don't
      // support any scaling modes except UNSPECIFIED anyway.
      displayMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
      displayMode.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
      
      if (FAILED(ChangeDisplayMode(output.ptr(), &displayMode))) {
        Logger::err("DXGI: EnterFullscreenMode: Failed to change display mode");
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
      }
    }
    
    // Update swap chain description
    m_descFs.Windowed = FALSE;
    
    // Change the window flags to remove the decoration etc.
    LONG style   = ::GetWindowLongW(m_window, GWL_STYLE);
    LONG exstyle = ::GetWindowLongW(m_window, GWL_EXSTYLE);
    
    m_windowState.style = style;
    m_windowState.exstyle = exstyle;
    
    style   &= ~WS_OVERLAPPEDWINDOW;
    exstyle &= ~WS_EX_OVERLAPPEDWINDOW;
    
    ::SetWindowLongW(m_window, GWL_STYLE, style);
    ::SetWindowLongW(m_window, GWL_EXSTYLE, exstyle);
    
    // Move the window so that it covers the entire output
    DXGI_OUTPUT_DESC desc;
    output->GetDesc(&desc);
    
    const RECT rect = desc.DesktopCoordinates;
    
    ::SetWindowPos(m_window, HWND_TOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    
    m_monitor = desc.Monitor;
    m_target  = std::move(output);

    // Apply current gamma curve of the output
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;

    if (SUCCEEDED(AcquireMonitorData(m_monitor, &monitorInfo))) {
      if (!monitorInfo->pSwapChain)
        monitorInfo->pSwapChain = this;
      
      SetGammaControl(DXGI_VK_GAMMA_CP_COUNT, monitorInfo->GammaCurve.GammaCurve);
      ReleaseMonitorData();
    }

    NotifyModeChange(m_monitor, FALSE);
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::LeaveFullscreenMode() {
    if (FAILED(RestoreDisplayMode(m_monitor)))
      Logger::warn("DXGI: LeaveFullscreenMode: Failed to restore display mode");
    
    // Reset gamma control and decouple swap chain from monitor
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;

    if (SUCCEEDED(AcquireMonitorData(m_monitor, &monitorInfo))) {
      if (monitorInfo->pSwapChain == this)
        monitorInfo->pSwapChain = nullptr;
      
      SetGammaControl(0, nullptr);
      ReleaseMonitorData();
    }
    
    // Restore internal state
    HMONITOR monitor = m_monitor;

    m_descFs.Windowed = TRUE;
    m_monitor = nullptr;
    m_target  = nullptr;
    
    if (!IsWindow(m_window))
      return S_OK;
    
    // Only restore the window style if the application hasn't
    // changed them. This is in line with what native DXGI does.
    LONG curStyle   = ::GetWindowLongW(m_window, GWL_STYLE) & ~WS_VISIBLE;
    LONG curExstyle = ::GetWindowLongW(m_window, GWL_EXSTYLE) & ~WS_EX_TOPMOST;
    
    if (curStyle == (m_windowState.style & ~(WS_VISIBLE | WS_OVERLAPPEDWINDOW))
     && curExstyle == (m_windowState.exstyle & ~(WS_EX_TOPMOST | WS_EX_OVERLAPPEDWINDOW))) {
      ::SetWindowLongW(m_window, GWL_STYLE,   m_windowState.style);
      ::SetWindowLongW(m_window, GWL_EXSTYLE, m_windowState.exstyle);
    }
    
    // Restore window position and apply the style
    const RECT rect = m_windowState.rect;
    
    ::SetWindowPos(m_window, (m_windowState.exstyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_NOACTIVATE);
    
    NotifyModeChange(monitor, TRUE);
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::ChangeDisplayMode(
          IDXGIOutput*            pOutput,
    const DXGI_MODE_DESC*         pDisplayMode) {
    if (!pOutput)
      return DXGI_ERROR_INVALID_CALL;
    
    // Find a mode that the output supports
    DXGI_OUTPUT_DESC outputDesc;
    pOutput->GetDesc(&outputDesc);
    
    DXGI_MODE_DESC preferredMode = *pDisplayMode;
    DXGI_MODE_DESC selectedMode;

    if (preferredMode.Format == DXGI_FORMAT_UNKNOWN)
      preferredMode.Format = m_desc.Format;
    
    HRESULT hr = pOutput->FindClosestMatchingMode(
      &preferredMode, &selectedMode, nullptr);
    
    if (FAILED(hr)) {
      Logger::err(str::format(
        "DXGI: Failed to query closest mode:",
        "\n  Format: ", preferredMode.Format,
        "\n  Mode:   ", preferredMode.Width, "x", preferredMode.Height,
          "@", preferredMode.RefreshRate.Numerator / preferredMode.RefreshRate.Denominator));
      return hr;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    devMode.dmPelsWidth  = selectedMode.Width;
    devMode.dmPelsHeight = selectedMode.Height;
    devMode.dmBitsPerPel = GetMonitorFormatBpp(selectedMode.Format);
    
    if (selectedMode.RefreshRate.Numerator != 0)  {
      devMode.dmFields |= DM_DISPLAYFREQUENCY;
      devMode.dmDisplayFrequency = selectedMode.RefreshRate.Numerator
                                 / selectedMode.RefreshRate.Denominator;
    }

    return SetMonitorDisplayMode(outputDesc.Monitor, &devMode)
      ? S_OK
      : DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
  }
  
  
  HRESULT DxgiSwapChain::RestoreDisplayMode(HMONITOR hMonitor) {
    if (!hMonitor)
      return DXGI_ERROR_INVALID_CALL;
    
    return RestoreMonitorDisplayMode()
      ? S_OK
      : DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
  }
  
  
  HRESULT DxgiSwapChain::GetSampleCount(UINT Count, VkSampleCountFlagBits* pCount) const {
    switch (Count) {
      case  1: *pCount = VK_SAMPLE_COUNT_1_BIT;  return S_OK;
      case  2: *pCount = VK_SAMPLE_COUNT_2_BIT;  return S_OK;
      case  4: *pCount = VK_SAMPLE_COUNT_4_BIT;  return S_OK;
      case  8: *pCount = VK_SAMPLE_COUNT_8_BIT;  return S_OK;
      case 16: *pCount = VK_SAMPLE_COUNT_16_BIT; return S_OK;
    }
    
    return E_INVALIDARG;
  }


  HRESULT DxgiSwapChain::GetOutputFromMonitor(
          HMONITOR                  Monitor,
          IDXGIOutput**             ppOutput) {
    if (!ppOutput)
      return DXGI_ERROR_INVALID_CALL;
    
    for (uint32_t i = 0; SUCCEEDED(m_adapter->EnumOutputs(i, ppOutput)); i++) {
      DXGI_OUTPUT_DESC outputDesc;
      (*ppOutput)->GetDesc(&outputDesc);
      
      if (outputDesc.Monitor == Monitor)
        return S_OK;
      
      (*ppOutput)->Release();
      (*ppOutput) = nullptr;
    }
    
    return DXGI_ERROR_NOT_FOUND;
  }


  HRESULT DxgiSwapChain::AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) {
    return m_monitorInfo != nullptr
      ? m_monitorInfo->AcquireMonitorData(hMonitor, ppData)
      : E_NOINTERFACE;
  }

  
  void DxgiSwapChain::ReleaseMonitorData() {
    if (m_monitorInfo != nullptr)
      m_monitorInfo->ReleaseMonitorData();
  }


  void DxgiSwapChain::NotifyModeChange(
          HMONITOR                hMonitor,
          BOOL                    Windowed) {
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

    if (GetMonitorDisplayMode(hMonitor, ENUM_CURRENT_SETTINGS, &devMode)) {
      DXGI_MODE_DESC displayMode = { };
      displayMode.Width            = devMode.dmPelsWidth;
      displayMode.Height           = devMode.dmPelsHeight;
      displayMode.RefreshRate      = { devMode.dmDisplayFrequency, 1 };
      displayMode.Format           = m_desc.Format;
      displayMode.ScanlineOrdering = m_descFs.ScanlineOrdering;
      displayMode.Scaling          = m_descFs.Scaling;
      m_presenter->NotifyModeChange(Windowed, &displayMode);
    } else {
      Logger::warn("Failed to query current display mode");
      m_presenter->NotifyModeChange(Windowed, nullptr);
    }
  }
  
}
