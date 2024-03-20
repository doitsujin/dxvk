#include "dxgi_factory.h"
#include "dxgi_output.h"
#include "dxgi_swapchain.h"

#include "../util/util_misc.h"

namespace dxvk {
  
  DxgiSwapChain::DxgiSwapChain(
          DxgiFactory*                pFactory,
          IDXGIVkSwapChain*           pPresenter,
          HWND                        hWnd,
    const DXGI_SWAP_CHAIN_DESC1*      pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*  pFullscreenDesc)
  : m_factory   (pFactory),
    m_window    (hWnd),
    m_desc      (*pDesc),
    m_descFs    (*pFullscreenDesc),
    m_presentId (0u),
    m_presenter (pPresenter),
    m_monitor   (wsi::getWindowMonitor(m_window)) {
    if (FAILED(m_presenter->GetAdapter(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&m_adapter))))
      throw DxvkError("DXGI: Failed to get adapter for present device");

    // Query updated interface versions from presenter, this
    // may fail e.g. with older vkd3d-proton builds.
    m_presenter->QueryInterface(__uuidof(IDXGIVkSwapChain1), reinterpret_cast<void**>(&m_presenter1));

    // Query monitor info form DXVK's DXGI factory, if available
    m_factory->QueryInterface(__uuidof(IDXGIVkMonitorInfo), reinterpret_cast<void**>(&m_monitorInfo));
    
    // Apply initial window mode and fullscreen state
    if (!m_descFs.Windowed && FAILED(EnterFullscreenMode(nullptr)))
      throw DxvkError("DXGI: Failed to set initial fullscreen state");

    // Ensure that RGBA16 swap chains are scRGB if supported
    UpdateColorSpace(m_desc.Format, m_colorSpace);
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    if (!m_descFs.Windowed)
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
    
    if (logQueryInterfaceError(__uuidof(IDXGISwapChain), riid)) {
      Logger::warn("DxgiSwapChain::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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
    
    if (!wsi::isWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;
    
    Com<IDXGIOutput1> output;

    if (m_target == nullptr) {
      HRESULT hr = GetOutputFromMonitor(wsi::getWindowMonitor(m_window), &output);

      if (FAILED(hr))
        return hr;
    } else {
      output = m_target;
    }

    *ppOutput = output.ref();
    return S_OK;
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
      Logger::warn("DxgiSwapChain::GetFrameStatistics: Frame statistics may be inaccurate");

    // Populate frame statistics with local present count and current time
    auto t1Counter = dxvk::high_resolution_clock::get_counter();

    DXGI_VK_FRAME_STATISTICS frameStatistics = { };
    frameStatistics.PresentCount = m_presentId;
    frameStatistics.PresentQPCTime = t1Counter;

    if (m_presenter1 != nullptr)
      m_presenter1->GetFrameStatistics(&frameStatistics);

    // Fill in actual DXGI statistics, using monitor data to help compute
    // vblank counts if possible. This is not fully accurate, especially on
    // displays with variable refresh rates, but it's the best we can do.
    DXGI_VK_MONITOR_DATA* monitorData = nullptr;

    pStats->PresentCount          = frameStatistics.PresentCount;
    pStats->PresentRefreshCount   = 0;
    pStats->SyncRefreshCount      = 0;
    pStats->SyncQPCTime.QuadPart  = frameStatistics.PresentQPCTime;
    pStats->SyncGPUTime.QuadPart  = 0;

    if (SUCCEEDED(AcquireMonitorData(m_monitor, &monitorData))) {
      auto refreshPeriod = computeRefreshPeriod(
        monitorData->LastMode.RefreshRate.Numerator,
        monitorData->LastMode.RefreshRate.Denominator);

      auto t0 = dxvk::high_resolution_clock::get_time_from_counter(monitorData->FrameStats.SyncQPCTime.QuadPart);
      auto t1 = dxvk::high_resolution_clock::get_time_from_counter(t1Counter);
      auto t2 = dxvk::high_resolution_clock::get_time_from_counter(frameStatistics.PresentQPCTime);

      pStats->PresentRefreshCount = m_presenter1 != nullptr
        ? monitorData->FrameStats.SyncRefreshCount + computeRefreshCount(t0, t2, refreshPeriod)
        : monitorData->FrameStats.PresentRefreshCount;
      pStats->SyncRefreshCount = monitorData->FrameStats.SyncRefreshCount + computeRefreshCount(t0, t1, refreshPeriod);

      ReleaseMonitorData();
    }

    // Docs say that DISJOINT is returned on the first call and around
    // mode changes. Just make this swap chain state for now.
    HRESULT hr = S_OK;

    if (std::exchange(m_frameStatisticsDisjoint, false))
      hr = DXGI_ERROR_FRAME_STATISTICS_DISJOINT;

    return hr;
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

    UINT64 presentId = m_presentId;

    if (m_presenter1 != nullptr)
      m_presenter1->GetLastPresentCount(&presentId);

    *pLastPresentCount = UINT(presentId);
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

    if (SyncInterval > 4)
      return DXGI_ERROR_INVALID_CALL;

    auto options = m_factory->GetOptions();

    if (options->syncInterval >= 0)
      SyncInterval = options->syncInterval;

    UpdateGlobalHDRState();

    std::lock_guard<dxvk::recursive_mutex> lockWin(m_lockWindow);
    HRESULT hr = S_OK;

    if (wsi::isWindow(m_window)) {
      std::lock_guard<dxvk::mutex> lockBuf(m_lockBuffer);
      hr = m_presenter->Present(SyncInterval, PresentFlags, nullptr);
    }

    if (PresentFlags & DXGI_PRESENT_TEST)
      return hr;

    if (hr == S_OK) {

      m_presentId += 1;

      // Update monitor frame statistics. This is not consistent with swap chain
      // frame statistics at all, but we want to ensure that all presents become
      // visible to the IDXGIOutput in case applications rely on that behaviour.
      DXGI_VK_MONITOR_DATA* monitorData = nullptr;

      if (SUCCEEDED(AcquireMonitorData(m_monitor, &monitorData))) {
        auto refreshPeriod = computeRefreshPeriod(
          monitorData->LastMode.RefreshRate.Numerator,
          monitorData->LastMode.RefreshRate.Denominator);

        auto t0 = dxvk::high_resolution_clock::get_time_from_counter(monitorData->FrameStats.SyncQPCTime.QuadPart);
        auto t1 = dxvk::high_resolution_clock::now();

        monitorData->FrameStats.PresentCount += 1;
        monitorData->FrameStats.PresentRefreshCount = monitorData->FrameStats.SyncRefreshCount + computeRefreshCount(t0, t1, refreshPeriod);
        ReleaseMonitorData();
      }
    }

    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeBuffers(
          UINT                      BufferCount,
          UINT                      Width,
          UINT                      Height,
          DXGI_FORMAT               NewFormat,
          UINT                      SwapChainFlags) {
    return ResizeBuffers1(BufferCount, Width, Height,
      NewFormat, SwapChainFlags, nullptr, nullptr);
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeBuffers1(
          UINT                      BufferCount,
          UINT                      Width,
          UINT                      Height,
          DXGI_FORMAT               Format,
          UINT                      SwapChainFlags,
    const UINT*                     pCreationNodeMask,
          IUnknown* const*          ppPresentQueue) {
    if (!wsi::isWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;

    constexpr UINT PreserveFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    if ((m_desc.Flags & PreserveFlags) != (SwapChainFlags & PreserveFlags))
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<dxvk::mutex> lock(m_lockBuffer);
    m_desc.Width  = Width;
    m_desc.Height = Height;
    
    wsi::getWindowSize(m_window,
      m_desc.Width  ? nullptr : &m_desc.Width,
      m_desc.Height ? nullptr : &m_desc.Height);
    
    if (BufferCount != 0)
      m_desc.BufferCount = BufferCount;
    
    if (Format != DXGI_FORMAT_UNKNOWN)
      m_desc.Format = Format;
    
    HRESULT hr = m_presenter->ChangeProperties(&m_desc, pCreationNodeMask, ppPresentQueue);

    if (FAILED(hr))
      return hr;

    UpdateColorSpace(m_desc.Format, m_colorSpace);
    return hr;
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);

    if (!pNewTargetParameters)
      return DXGI_ERROR_INVALID_CALL;
    
    if (!wsi::isWindow(m_window))
      return DXGI_ERROR_INVALID_CALL;

    // Promote display mode
    DXGI_MODE_DESC1 newDisplayMode = { };
    newDisplayMode.Width = pNewTargetParameters->Width;
    newDisplayMode.Height = pNewTargetParameters->Height;
    newDisplayMode.RefreshRate = pNewTargetParameters->RefreshRate;
    newDisplayMode.Format = pNewTargetParameters->Format;
    newDisplayMode.ScanlineOrdering = pNewTargetParameters->ScanlineOrdering;
    newDisplayMode.Scaling = pNewTargetParameters->Scaling;

    // Update the swap chain description
    if (newDisplayMode.RefreshRate.Numerator != 0)
      m_descFs.RefreshRate = newDisplayMode.RefreshRate;
    
    m_descFs.ScanlineOrdering = newDisplayMode.ScanlineOrdering;
    m_descFs.Scaling          = newDisplayMode.Scaling;
    
    if (m_descFs.Windowed) {
      wsi::resizeWindow(
        m_window, &m_windowState,
        newDisplayMode.Width,
        newDisplayMode.Height);
    } else {
      Com<IDXGIOutput1> output;
      
      if (FAILED(GetOutputFromMonitor(m_monitor, &output))) {
        Logger::err("DXGI: ResizeTarget: Failed to query containing output");
        return E_FAIL;
      }
      
      // If the swap chain allows it, change the display mode
      if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
        ChangeDisplayMode(output.ptr(), &newDisplayMode);

      wsi::updateFullscreenWindow(m_monitor, m_window, false);
    }
    
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    std::lock_guard<dxvk::recursive_mutex> lock(m_lockWindow);

    if (!Fullscreen && pTarget)
      return DXGI_ERROR_INVALID_CALL;

    Com<IDXGIOutput1> target;

    if (pTarget) {
      DXGI_OUTPUT_DESC desc;

      pTarget->QueryInterface(IID_PPV_ARGS(&target));
      target->GetDesc(&desc);

      if (!m_descFs.Windowed && Fullscreen && m_monitor != desc.Monitor) {
        HRESULT hr = this->LeaveFullscreenMode();
        if (FAILED(hr))
          return hr;
      }
    }

    if (m_descFs.Windowed && Fullscreen)
      return this->EnterFullscreenMode(target.ptr());
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

    std::lock_guard<dxvk::mutex> lock(m_lockBuffer);

    RECT region = { 0, 0, LONG(Width), LONG(Height) };
    return m_presenter->SetPresentRegion(&region);
  }
  

  HRESULT STDMETHODCALLTYPE DxgiSwapChain::CheckColorSpaceSupport(
          DXGI_COLOR_SPACE_TYPE           ColorSpace,
          UINT*                           pColorSpaceSupport) {
    if (!pColorSpaceSupport)
      return E_INVALIDARG;

    std::lock_guard<dxvk::mutex> lock(m_lockBuffer);

    if (ValidateColorSpaceSupport(m_desc.Format, ColorSpace))
      *pColorSpaceSupport = DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT;
    else
      *pColorSpaceSupport = 0;

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) {
    std::lock_guard<dxvk::mutex> lock(m_lockBuffer);

    if (!ValidateColorSpaceSupport(m_desc.Format, ColorSpace))
      return E_INVALIDARG;

    // Write back color space if setting it up succeeded. This way, we preserve
    // the current color space even if the swap chain temporarily switches to a
    // back buffer format which does not support it.
    HRESULT hr = UpdateColorSpace(m_desc.Format, ColorSpace);

    if (SUCCEEDED(hr))
      m_colorSpace = ColorSpace;

    return hr;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetHDRMetaData(
          DXGI_HDR_METADATA_TYPE    Type,
          UINT                      Size,
          void*                     pMetaData) {
    if (Size && !pMetaData)
      return E_INVALIDARG;

    DXGI_VK_HDR_METADATA metadata = { Type };

    switch (Type) {
      case DXGI_HDR_METADATA_TYPE_NONE:
        break;

      case DXGI_HDR_METADATA_TYPE_HDR10:
        if (Size != sizeof(DXGI_HDR_METADATA_HDR10))
          return E_INVALIDARG;

        metadata.HDR10 = *static_cast<const DXGI_HDR_METADATA_HDR10*>(pMetaData);
        break;

      default:
        Logger::err(str::format("DXGI: Unsupported HDR metadata type: ", Type));
        return E_INVALIDARG;
    }

    std::lock_guard<dxvk::mutex> lock(m_lockBuffer);
    return m_presenter->SetHDRMetaData(&metadata);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetGammaControl(
          UINT                      NumPoints,
    const DXGI_RGB*                 pGammaCurve) {
    std::lock_guard<dxvk::mutex> lockBuf(m_lockBuffer);
    return m_presenter->SetGammaControl(NumPoints, pGammaCurve);
  }


  HRESULT DxgiSwapChain::EnterFullscreenMode(IDXGIOutput1* pTarget) {
    Com<IDXGIOutput1> output = pTarget;

    if (!wsi::isWindow(m_window))
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    
    if (output == nullptr) {
      if (FAILED(GetOutputFromMonitor(wsi::getWindowMonitor(m_window), &output))) {
        Logger::err("DXGI: EnterFullscreenMode: Cannot query containing output");
        return E_FAIL;
      }
    }

    const bool modeSwitch = m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    
    if (modeSwitch) {
      DXGI_MODE_DESC1 displayMode = { };
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
    
    // Move the window so that it covers the entire output
    DXGI_OUTPUT_DESC desc;
    output->GetDesc(&desc);

    if (!wsi::enterFullscreenMode(desc.Monitor, m_window, &m_windowState, modeSwitch)) {
        Logger::err("DXGI: EnterFullscreenMode: Failed to enter fullscreen mode");
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    
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
    m_descFs.Windowed = TRUE;
    m_target  = nullptr;
    m_monitor = wsi::getWindowMonitor(m_window);
    
    if (!wsi::isWindow(m_window))
      return S_OK;
    
    if (!wsi::leaveFullscreenMode(m_window, &m_windowState, true)) {
      Logger::err("DXGI: LeaveFullscreenMode: Failed to exit fullscreen mode");
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::ChangeDisplayMode(
          IDXGIOutput1*           pOutput,
    const DXGI_MODE_DESC1*        pDisplayMode) {
    if (!pOutput)
      return DXGI_ERROR_INVALID_CALL;
    
    // Find a mode that the output supports
    DXGI_OUTPUT_DESC outputDesc;
    pOutput->GetDesc(&outputDesc);
    
    DXGI_MODE_DESC1 preferredMode = *pDisplayMode;
    DXGI_MODE_DESC1 selectedMode;

    if (preferredMode.Format == DXGI_FORMAT_UNKNOWN)
      preferredMode.Format = m_desc.Format;
    
    HRESULT hr = pOutput->FindClosestMatchingMode1(
      &preferredMode, &selectedMode, nullptr);
    
    if (FAILED(hr)) {
      Logger::err(str::format(
        "DXGI: Failed to query closest mode:",
        "\n  Format: ", preferredMode.Format,
        "\n  Mode:   ", preferredMode.Width, "x", preferredMode.Height,
          "@", preferredMode.RefreshRate.Numerator / std::max(preferredMode.RefreshRate.Denominator, 1u)));
      return hr;
    }

    if (!wsi::setWindowMode(outputDesc.Monitor, m_window, ConvertDisplayMode(selectedMode)))
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;

    DXGI_VK_MONITOR_DATA* monitorData = nullptr;

    if (SUCCEEDED(AcquireMonitorData(outputDesc.Monitor, &monitorData))) {
      auto refreshPeriod = computeRefreshPeriod(
        monitorData->LastMode.RefreshRate.Numerator,
        monitorData->LastMode.RefreshRate.Denominator);

      auto t1Counter = dxvk::high_resolution_clock::get_counter();

      auto t0 = dxvk::high_resolution_clock::get_time_from_counter(monitorData->FrameStats.SyncQPCTime.QuadPart);
      auto t1 = dxvk::high_resolution_clock::get_time_from_counter(t1Counter);

      monitorData->FrameStats.SyncRefreshCount += computeRefreshCount(t0, t1, refreshPeriod);
      monitorData->FrameStats.SyncQPCTime.QuadPart = t1Counter;
      monitorData->LastMode = selectedMode;
      ReleaseMonitorData();
    }

    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::RestoreDisplayMode(HMONITOR hMonitor) {
    if (!hMonitor)
      return DXGI_ERROR_INVALID_CALL;
    
    if (!wsi::restoreDisplayMode())
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;

    return S_OK;
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
          IDXGIOutput1**            ppOutput) {
    if (!ppOutput)
      return DXGI_ERROR_INVALID_CALL;

    Com<IDXGIOutput> output;

    for (uint32_t i = 0; SUCCEEDED(m_adapter->EnumOutputs(i, &output)); i++) {
      DXGI_OUTPUT_DESC outputDesc;
      output->GetDesc(&outputDesc);
      
      if (outputDesc.Monitor == Monitor)
        return output->QueryInterface(IID_PPV_ARGS(ppOutput));
      
      output = nullptr;
    }
    
    return DXGI_ERROR_NOT_FOUND;
  }


  HRESULT DxgiSwapChain::AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) {
    if (m_monitorInfo == nullptr || !hMonitor)
      return E_NOINTERFACE;

    HRESULT hr = m_monitorInfo->AcquireMonitorData(hMonitor, ppData);

    if (FAILED(hr) && HasLiveReferences()) {
      // We may need to initialize a DXGI output to populate monitor data.
      // If acquiring monitor data has failed previously, do not try again.
      if (hMonitor == m_monitor && !m_monitorHasOutput)
        return E_NOINTERFACE;

      Com<IDXGIOutput1> output;

      if (SUCCEEDED(GetOutputFromMonitor(hMonitor, &output)))
        hr = m_monitorInfo->AcquireMonitorData(hMonitor, ppData);
    }

    if (hMonitor == m_monitor)
      m_monitorHasOutput = SUCCEEDED(hr);

    return hr;
  }

  
  void DxgiSwapChain::ReleaseMonitorData() {
    if (m_monitorInfo != nullptr)
      m_monitorInfo->ReleaseMonitorData();
  }


  void DxgiSwapChain::UpdateGlobalHDRState() {
    // Update the global HDR state if called from the legacy NVAPI
    // interfaces, etc.

    auto state = m_factory->GlobalHDRState();
    if (m_globalHDRStateSerial != state.Serial) {
      SetColorSpace1(state.ColorSpace);

      switch (state.Metadata.Type) {
        case DXGI_HDR_METADATA_TYPE_NONE:
          SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
          break;
        case DXGI_HDR_METADATA_TYPE_HDR10:
          SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(state.Metadata.HDR10), reinterpret_cast<void*>(&state.Metadata.HDR10));
          break;
        default:
          Logger::err(str::format("DXGI: Unsupported HDR metadata type (global): ", state.Metadata.Type));
          break;
      }

      m_globalHDRStateSerial = state.Serial;
    }
  }


  bool DxgiSwapChain::ValidateColorSpaceSupport(
          DXGI_FORMAT             Format,
          DXGI_COLOR_SPACE_TYPE   ColorSpace) {
    // RGBA16 swap chains are treated as scRGB even on SDR displays,
    // and regular sRGB is not exposed when this format is used.
    if (Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
      return ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;

    // For everything else, we will always expose plain sRGB
    if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
      return true;

    // Only expose HDR10 color space if HDR option is enabled
    if (ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
      return m_factory->GetOptions()->enableHDR && m_presenter->CheckColorSpaceSupport(ColorSpace);

    return false;
  }


  HRESULT DxgiSwapChain::UpdateColorSpace(
          DXGI_FORMAT             Format,
          DXGI_COLOR_SPACE_TYPE   ColorSpace) {
    // Don't do anything if the explicitly sepected color space
    // is compatible with the back buffer format already
    if (!ValidateColorSpaceSupport(Format, ColorSpace)) {
      ColorSpace = Format == DXGI_FORMAT_R16G16B16A16_FLOAT
        ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709
        : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    }

    // Ensure that we pick a supported color space. This is relevant for
    // mapping scRGB to sRGB on SDR setups, matching Windows behaviour.
    if (!m_presenter->CheckColorSpaceSupport(ColorSpace))
      ColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    HRESULT hr = m_presenter->SetColorSpace(ColorSpace);

    // If this was a colorspace other than our current one,
    // punt us into that one on the DXGI output.
    if (SUCCEEDED(hr))
      m_monitorInfo->PuntColorSpace(ColorSpace);

    return hr;
  }

}
