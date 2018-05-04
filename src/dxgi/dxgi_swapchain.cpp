#include "dxgi_device.h"
#include "dxgi_factory.h"
#include "dxgi_output.h"
#include "dxgi_swapchain.h"

namespace dxvk {
  
  DxgiSwapChain::DxgiSwapChain(
          DxgiFactory*          factory,
          IUnknown*             pDevice,
          DXGI_SWAP_CHAIN_DESC* pDesc)
  : m_factory (factory),
    m_desc    (*pDesc),
    m_monitor (nullptr) {
    
    // Retrieve a device pointer that allows us to
    // communicate with the underlying D3D device
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIVkPresenter),
        reinterpret_cast<void**>(&m_presentDevice))))
      throw DxvkError("DXGI: DxgiSwapChain: Invalid device");
    
    // Retrieve the adapter, which is going
    // to be used to enumerate displays.
    Com<IDXGIDevice> device;
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&device))))
      throw DxvkError("DXGI: DxgiSwapChain: Invalid device");
    
    if (FAILED(device->GetAdapter(&adapter)))
      throw DxvkError("DXGI: DxgiSwapChain: Failed to retrieve adapter");
    
    m_device  = static_cast<DxgiDevice*>(device.ptr());
    m_adapter = static_cast<DxgiAdapter*>(adapter.ptr());
    
    // Initialize frame statistics
    m_stats.PresentCount         = 0;
    m_stats.PresentRefreshCount  = 0;
    m_stats.SyncRefreshCount     = 0;
    m_stats.SyncQPCTime.QuadPart = 0;
    m_stats.SyncGPUTime.QuadPart = 0;
    
    // Adjust initial back buffer size. If zero, these
    // shall be set to the current window size.
    const VkExtent2D windowSize = GetWindowSize();
    
    if (m_desc.BufferDesc.Width  == 0) m_desc.BufferDesc.Width  = windowSize.width;
    if (m_desc.BufferDesc.Height == 0) m_desc.BufferDesc.Height = windowSize.height;
    
    // Set initial window mode and fullscreen state
    if (!pDesc->Windowed && FAILED(EnterFullscreenMode(nullptr)))
      throw DxvkError("DXGI: DxgiSwapChain: Failed to set initial fullscreen state");
    
    if (FAILED(CreatePresenter()) || FAILED(CreateBackBuffer()))
      throw DxvkError("DXGI: DxgiSwapChain: Failed to create presenter or back buffer");
    
    if (FAILED(SetDefaultGammaControl()))
      throw DxvkError("DXGI: DxgiSwapChain: Failed to set up gamma ramp");
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    Com<IDXGIOutput> output;
    
    if (SUCCEEDED(m_adapter->GetOutputFromMonitor(m_monitor, &output)))
      RestoreDisplayMode(output.ptr());
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDeviceSubObject)
     || riid == __uuidof(IDXGISwapChain)) {
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
    return m_device->QueryInterface(riid, ppDevice);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    InitReturnPtr(ppSurface);
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    if (Buffer > 0) {
      Logger::err("DxgiSwapChain::GetBuffer: Buffer > 0 not supported");
      return DXGI_ERROR_INVALID_CALL;
    }
    
    return m_backBuffer->QueryInterface(riid, ppSurface);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    InitReturnPtr(ppOutput);
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    RECT windowRect = { 0, 0, 0, 0 };
    ::GetWindowRect(m_desc.OutputWindow, &windowRect);
    
    HMONITOR monitor = ::MonitorFromPoint(
      { (windowRect.left + windowRect.right) / 2,
        (windowRect.top + windowRect.bottom) / 2 },
      MONITOR_DEFAULTTOPRIMARY);
    
    return m_adapter->GetOutputFromMonitor(monitor, ppOutput);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pDesc = m_desc;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (pStats == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pStats = m_stats;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    HRESULT hr = S_OK;
    
    if (pFullscreen != nullptr)
      *pFullscreen = !m_desc.Windowed;
    
    if (ppTarget != nullptr) {
      *ppTarget = nullptr;
      
      if (!m_desc.Windowed)
        hr = m_adapter->GetOutputFromMonitor(m_monitor, ppTarget);
    }
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (pLastPresentCount == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pLastPresentCount = m_stats.PresentCount;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    if (Flags & DXGI_PRESENT_TEST)
      return S_OK;
    
    try {
      // If in fullscreen mode, apply any updated gamma curve
      // if it has been changed since the last present call.
      DXGI_VK_OUTPUT_DATA outputData;
      
      if (SUCCEEDED(m_adapter->GetOutputData(m_monitor, &outputData)) && outputData.GammaDirty) {
        SetGammaControl(&outputData.GammaCurve);
        
        outputData.GammaDirty = FALSE;
        m_adapter->SetOutputData(m_monitor, &outputData);
      }
      
      // Submit pending rendering commands
      // before recording the present code.
      m_presentDevice->FlushRenderingCommands();
      
      // Update swap chain properties. This will not only set
      // up vertical synchronization properly, but also apply
      // changes that were made to the window size even if the
      // Vulkan swap chain itself remains valid.
      DxvkSwapchainProperties swapchainProps;
      swapchainProps.preferredSurfaceFormat
        = m_presenter->PickSurfaceFormat(m_desc.BufferDesc.Format);
      swapchainProps.preferredPresentMode = SyncInterval == 0
        ? m_presenter->PickPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        : m_presenter->PickPresentMode(VK_PRESENT_MODE_FIFO_KHR);
      swapchainProps.preferredBufferSize = GetWindowSize();
      
      m_presenter->RecreateSwapchain(&swapchainProps);
      m_presenter->PresentImage();
      return S_OK;
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    const VkExtent2D windowSize = GetWindowSize();
    
    m_desc.BufferDesc.Width  = Width  != 0 ? Width  : windowSize.width;
    m_desc.BufferDesc.Height = Height != 0 ? Height : windowSize.height;
    
    if (BufferCount != 0)
      m_desc.BufferCount = BufferCount;
    
    if (NewFormat != DXGI_FORMAT_UNKNOWN)
      m_desc.BufferDesc.Format = NewFormat;
    
    return CreateBackBuffer();
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (pNewTargetParameters == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    // Update the swap chain description
    if (pNewTargetParameters->RefreshRate.Numerator != 0)
      m_desc.BufferDesc.RefreshRate = pNewTargetParameters->RefreshRate;
    
    m_desc.BufferDesc.ScanlineOrdering = pNewTargetParameters->ScanlineOrdering;
    m_desc.BufferDesc.Scaling          = pNewTargetParameters->Scaling;
    
    if (m_desc.Windowed) {
      // Adjust window position and size
      RECT newRect = { 0, 0, 0, 0 };
      RECT oldRect = { 0, 0, 0, 0 };
      
      ::GetWindowRect(m_desc.OutputWindow, &oldRect);
      ::SetRect(&newRect, 0, 0, pNewTargetParameters->Width, pNewTargetParameters->Height);
      ::AdjustWindowRectEx(&newRect,
        ::GetWindowLongW(m_desc.OutputWindow, GWL_STYLE), FALSE,
        ::GetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE));
      ::SetRect(&newRect, 0, 0, newRect.right - newRect.left, newRect.bottom - newRect.top);
      ::OffsetRect(&newRect, oldRect.left, oldRect.top);    
      ::MoveWindow(m_desc.OutputWindow, newRect.left, newRect.top,
          newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
    } else {
      Com<IDXGIOutput> output;
      
      if (FAILED(m_adapter->GetOutputFromMonitor(m_monitor, &output))) {
        Logger::err("DXGI: ResizeTarget: Failed to query containing output");
        return E_FAIL;
      }
      
      // If the swap chain allows it, change the display mode
      if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)
        ChangeDisplayMode(output.ptr(), pNewTargetParameters);
      
      // Resize and reposition the window to 
      DXGI_OUTPUT_DESC desc;
      output->GetDesc(&desc);
      
      const RECT newRect = desc.DesktopCoordinates;
      
      ::MoveWindow(m_desc.OutputWindow, newRect.left, newRect.top,
          newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
    }
    
    
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (!IsWindow(m_desc.OutputWindow))
      return DXGI_ERROR_INVALID_CALL;
    
    if (m_desc.Windowed && Fullscreen)
      return this->EnterFullscreenMode(pTarget);
    else if (!m_desc.Windowed && !Fullscreen)
      return this->LeaveFullscreenMode();
    
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::SetGammaControl(const DXGI_GAMMA_CONTROL* pGammaControl) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    DXGI_VK_GAMMA_CURVE curve;
    
    for (uint32_t i = 0; i < DXGI_VK_GAMMA_CP_COUNT; i++) {
      const DXGI_RGB cp = pGammaControl->GammaCurve[i];
      curve.ControlPoints[i].R = MapGammaControlPoint(cp.Red);
      curve.ControlPoints[i].G = MapGammaControlPoint(cp.Green);
      curve.ControlPoints[i].B = MapGammaControlPoint(cp.Blue);
      curve.ControlPoints[i].A = 0;
    }
    
    m_presenter->SetGammaControl(&curve);
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::SetDefaultGammaControl() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    DXGI_VK_GAMMA_CURVE curve;
    
    for (uint32_t i = 0; i < DXGI_VK_GAMMA_CP_COUNT; i++) {
      const uint16_t value = MapGammaControlPoint(
        float(i) / float(DXGI_VK_GAMMA_CP_COUNT - 1));
      curve.ControlPoints[i] = { value, value, value, 0 };
    }
    
    m_presenter->SetGammaControl(&curve);
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::CreatePresenter() {
    try {
      m_presenter = new DxgiVkPresenter(
        m_device->GetDXVKDevice(),
        m_desc.OutputWindow);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT DxgiSwapChain::CreateBackBuffer() {
    // Figure out sample count based on swap chain description
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    
    if (FAILED(GetSampleCount(m_desc.SampleDesc.Count, &sampleCount))) {
      Logger::err("DXGI: CreateBackBuffer: Invalid sample count");
      return E_INVALIDARG;
    }
    
    // Destroy previous back buffer before creating a new one
    m_backBuffer = nullptr;
    
    if (FAILED(m_presentDevice->CreateSwapChainBackBuffer(&m_desc, &m_backBuffer))) {
      Logger::err("DXGI: CreateBackBuffer: Failed to create back buffer");
      return E_FAIL;
    }
    
    try {
      m_presenter->UpdateBackBuffer(m_backBuffer->GetDXVKImage());
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  VkExtent2D DxgiSwapChain::GetWindowSize() const {
    RECT windowRect;
    
    if (!::GetClientRect(m_desc.OutputWindow, &windowRect))
      windowRect = RECT();
    
    VkExtent2D result;
    result.width  = windowRect.right;
    result.height = windowRect.bottom;
    return result;
  }
  
  
  HRESULT DxgiSwapChain::EnterFullscreenMode(IDXGIOutput* pTarget) {
    Com<IDXGIOutput> output = static_cast<DxgiOutput*>(pTarget);
    
    if (output == nullptr) {
      if (FAILED(GetContainingOutput(&output))) {
        Logger::err("DXGI: EnterFullscreenMode: Cannot query containing output");
        return E_FAIL;
      }
    }
    
    // Find a display mode that matches what we need
    ::GetWindowRect(m_desc.OutputWindow, &m_windowState.rect);
    
    if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH) {
      auto windowRect = m_windowState.rect;
      
      DXGI_MODE_DESC displayMode = m_desc.BufferDesc;
      displayMode.Width  = windowRect.right - windowRect.left;
      displayMode.Height = windowRect.bottom - windowRect.top;
      
      if (FAILED(ChangeDisplayMode(output.ptr(), &displayMode))) {
        Logger::err("DXGI: EnterFullscreenMode: Failed to change display mode");
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
      }
    }
    
    // Update swap chain description
    m_desc.Windowed = FALSE;
    
    // Change the window flags to remove the decoration etc.
    LONG style   = ::GetWindowLongW(m_desc.OutputWindow, GWL_STYLE);
    LONG exstyle = ::GetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE);
    
    m_windowState.style = style;
    m_windowState.exstyle = exstyle;
    
    style   &= ~WS_OVERLAPPEDWINDOW;
    exstyle &= ~WS_EX_OVERLAPPEDWINDOW;
    
    ::SetWindowLongW(m_desc.OutputWindow, GWL_STYLE, style);
    ::SetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE, exstyle);
    
    // Move the window so that it covers the entire output
    DXGI_OUTPUT_DESC desc;
    output->GetDesc(&desc);
    
    const RECT rect = desc.DesktopCoordinates;
    
    ::SetWindowPos(m_desc.OutputWindow, HWND_TOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    
    m_monitor = desc.Monitor;
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::LeaveFullscreenMode() {
    Com<IDXGIOutput> output;
    
    if (FAILED(m_adapter->GetOutputFromMonitor(m_monitor, &output))
     || FAILED(RestoreDisplayMode(output.ptr())))
      Logger::warn("DXGI: LeaveFullscreenMode: Failed to restore display mode");
    
    // Restore internal state
    m_desc.Windowed = TRUE;
    m_monitor = nullptr;
    
    // Only restore the window style if the application hasn't
    // changed them. This is in line with what native DXGI does.
    LONG curStyle   = ::GetWindowLongW(m_desc.OutputWindow, GWL_STYLE) & ~WS_VISIBLE;
    LONG curExstyle = ::GetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE) & ~WS_EX_TOPMOST;
    
    if (curStyle == (m_windowState.style & ~(WS_VISIBLE | WS_OVERLAPPEDWINDOW))
     && curExstyle == (m_windowState.exstyle & ~(WS_EX_TOPMOST | WS_EX_OVERLAPPEDWINDOW))) {
      ::SetWindowLongW(m_desc.OutputWindow, GWL_STYLE,   m_windowState.style);
      ::SetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE, m_windowState.exstyle);
    }
    
    // Restore window position and apply the style
    const RECT rect = m_windowState.rect;
    
    ::SetWindowPos(m_desc.OutputWindow, 0,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    
    return SetDefaultGammaControl();
  }
  
  
  HRESULT DxgiSwapChain::ChangeDisplayMode(
          IDXGIOutput*            pOutput,
    const DXGI_MODE_DESC*         pDisplayMode) {
    auto output = static_cast<DxgiOutput*>(pOutput);
    
    if (output == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // Find a mode that the output supports
    DXGI_MODE_DESC selectedMode;
    
    HRESULT hr = output->FindClosestMatchingMode(
      pDisplayMode, &selectedMode, nullptr);
    
    if (FAILED(hr))
      return hr;
    
    return output->SetDisplayMode(&selectedMode);
  }
  
  
  HRESULT DxgiSwapChain::RestoreDisplayMode(IDXGIOutput* pOutput) {
    auto output = static_cast<DxgiOutput*>(pOutput);
    
    if (output == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // Restore registry settings
    DXGI_MODE_DESC mode;
    
    HRESULT hr = output->GetDisplayMode(
      &mode, ENUM_REGISTRY_SETTINGS);
    
    if (FAILED(hr))
      return hr;
    
    return output->SetDisplayMode(&mode);
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
  
}
