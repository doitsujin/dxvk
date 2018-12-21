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
    m_presenter (pPresenter),
    m_monitor   (nullptr) {
    // Initialize frame statistics
    m_stats.PresentCount         = 0;
    m_stats.PresentRefreshCount  = 0;
    m_stats.SyncRefreshCount     = 0;
    m_stats.SyncQPCTime.QuadPart = 0;
    m_stats.SyncGPUTime.QuadPart = 0;
    
    if (FAILED(m_presenter->GetAdapter(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&m_adapter))))
      throw DxvkError("DXGI: Failed to get adapter for present device");
    
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
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDeviceSubObject)
     || riid == __uuidof(IDXGISwapChain)
     || riid == __uuidof(IDXGISwapChain1)
     || riid == __uuidof(IDXGISwapChain2)
     || riid == __uuidof(IDXGISwapChain3)) {
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
    
    RECT windowRect = { 0, 0, 0, 0 };
    ::GetWindowRect(m_window, &windowRect);
    
    HMONITOR monitor = ::MonitorFromPoint(
      { (windowRect.left + windowRect.right) / 2,
        (windowRect.top + windowRect.bottom) / 2 },
      MONITOR_DEFAULTTOPRIMARY);
    
    return GetOutputFromMonitor(monitor, ppOutput);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
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
      return DXGI_ERROR_INVALID_CALL;
    
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
    if (pStats == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pStats = m_stats;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    if (!IsWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;
    
    HRESULT hr = S_OK;
    
    if (pFullscreen != nullptr)
      *pFullscreen = !m_descFs.Windowed;
    
    if (ppTarget != nullptr) {
      *ppTarget = nullptr;
      
      if (!m_descFs.Windowed)
        hr = GetOutputFromMonitor(m_monitor, ppTarget);
    }
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenDesc(
          DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pDesc = m_descFs;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetHwnd(
          HWND*                     pHwnd) {
    if (pHwnd == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
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
      return DXGI_ERROR_INVALID_CALL;
    
    *pLastPresentCount = m_stats.PresentCount;
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
    
    if (PresentFlags & DXGI_PRESENT_TEST)
      return S_OK;
    
    std::lock_guard<std::mutex> lockWin(m_lockWindow);
    std::lock_guard<std::mutex> lockBuf(m_lockBuffer);

    // Higher values are not allowed according to the Microsoft documentation:
    // 
    //   "1 through 4 - Synchronize presentation after the nth vertical blank."
    //   https://msdn.microsoft.com/en-us/library/windows/desktop/bb174576(v=vs.85).aspx
    SyncInterval = std::min<UINT>(SyncInterval, 4);

    try {
      return m_presenter->Present(SyncInterval, PresentFlags, nullptr);
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
    
    std::lock_guard<std::mutex> lock(m_lockBuffer);
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
    std::lock_guard<std::mutex> lock(m_lockWindow);

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
      if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
        ChangeDisplayMode(output.ptr(), pNewTargetParameters);
      
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
    std::lock_guard<std::mutex> lock(m_lockWindow);

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
    Logger::err("DxgiSwapChain::GetFrameLatencyWaitableObject: Not implemented");
    return nullptr;
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetMatrixTransform(
          DXGI_MATRIX_3X2_F*        pMatrix) {
    // We don't support composition swap chains
    Logger::err("DxgiSwapChain::GetMatrixTransform: Not supported");
    return DXGI_ERROR_INVALID_CALL;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetMaximumFrameLatency(
          UINT*                     pMaxLatency) {
    Logger::err("DxgiSwapChain::GetMaximumFrameLatency: Not implemented");
    return DXGI_ERROR_INVALID_CALL;
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
    Logger::err("DxgiSwapChain::SetMaximumFrameLatency: Not implemented");
    return DXGI_ERROR_INVALID_CALL;
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
    Logger::err("DxgiSwapChain::CheckColorSpaceSupport: Not implemented");

    *pColorSpaceSupport = 0;
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) {
    Logger::err("DxgiSwapChain::SetColorSpace1: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetGammaControl(
          UINT                      NumPoints,
    const DXGI_RGB*                 pGammaCurve) {
    std::lock_guard<std::mutex> lockBuf(m_lockBuffer);
    return m_presenter->SetGammaControl(NumPoints, pGammaCurve);
  }


  HRESULT DxgiSwapChain::EnterFullscreenMode(IDXGIOutput* pTarget) {
    Com<IDXGIOutput> output = static_cast<DxgiOutput*>(pTarget);

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

    // Apply current gamma curve of the output
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;

    if (SUCCEEDED(AcquireMonitorData(m_monitor, &monitorInfo))) {
      if (!monitorInfo->pSwapChain)
        monitorInfo->pSwapChain = this;
      
      SetGammaControl(DXGI_VK_GAMMA_CP_COUNT, monitorInfo->GammaCurve.GammaCurve);
      ReleaseMonitorData();
    }

    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::LeaveFullscreenMode() {
    if (!IsWindow(m_window))
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    
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
    m_descFs.Windowed = TRUE;
    m_monitor = nullptr;
    
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
    
    ::SetWindowPos(m_window, 0,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    
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
    
    return SetMonitorDisplayMode(outputDesc.Monitor, &selectedMode);
  }
  
  
  HRESULT DxgiSwapChain::RestoreDisplayMode(HMONITOR hMonitor) {
    if (!hMonitor)
      return DXGI_ERROR_INVALID_CALL;
    
    // Restore registry settings
    DXGI_MODE_DESC mode;
    
    HRESULT hr = GetMonitorDisplayMode(
      hMonitor, ENUM_REGISTRY_SETTINGS, &mode);
    
    if (FAILED(hr))
      return hr;
    
    return SetMonitorDisplayMode(hMonitor, &mode);
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
  
  
  HRESULT CreateDxvkSwapChainForHwnd(
          IDXGIFactory*             pFactory,
          IDXGIVkPresentDevice*     pDevice,
          HWND                      hWnd,
    const DXGI_SWAP_CHAIN_DESC1*    pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
          IDXGIOutput*              pRestrictToOutput,
          IDXGISwapChain1**         ppSwapChain) {
    // Make sure the back buffer size is not zero
    DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
    
    GetWindowClientSize(hWnd,
      desc.Width  ? nullptr : &desc.Width,
      desc.Height ? nullptr : &desc.Height);
    
    // If necessary, set up a default set of
    // fullscreen parameters for the swap chain
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;
    
    if (pFullscreenDesc) {
      fsDesc = *pFullscreenDesc;
    } else {
      fsDesc.RefreshRate      = { 0, 0 };
      fsDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
      fsDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
      fsDesc.Windowed         = TRUE;
    }
    
    // Create presenter for the device
    Com<IDXGIVkSwapChain> presenter;
    
    HRESULT hr = pDevice->CreateSwapChainForHwnd(
      hWnd, &desc, &presenter);
    
    if (FAILED(hr))
      return hr;
    
    try {
      // Create actual swap chain object
      *ppSwapChain = ref(new DxgiSwapChain(
        pFactory, presenter.ptr(), hWnd, &desc, &fsDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
  
}
