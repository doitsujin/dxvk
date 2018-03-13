#include "dxgi_device.h"
#include "dxgi_factory.h"
#include "dxgi_swapchain.h"

namespace dxvk {
  
  DxgiSwapChain::DxgiSwapChain(
          DxgiFactory*          factory,
          IUnknown*             pDevice,
          DXGI_SWAP_CHAIN_DESC* pDesc)
  : m_factory (factory),
    m_desc    (*pDesc) {
    
    // Retrieve a device pointer that allows us to
    // communicate with the underlying D3D device
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIPresentDevicePrivate),
        reinterpret_cast<void**>(&m_presentDevice))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    // Retrieve the adapter, which is going
    // to be used to enumerate displays.
    Com<IDXGIDevice> device;
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&device))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    if (FAILED(device->GetAdapter(&adapter)))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to retrieve adapter");
    
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
      throw DxvkError("DxgiSwapChain: Failed to set initial fullscreen state");
    
    if (FAILED(CreatePresenter()) || FAILED(CreateBackBuffer()))
      throw DxvkError("DxgiSwapChain: Failed to create presenter or back buffer");
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDeviceSubObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGISwapChain);
    
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (Buffer > 0) {
      Logger::err("DxgiSwapChain::GetBuffer: Buffer > 0 not supported");
      return DXGI_ERROR_INVALID_CALL;
    }
    
    return m_backBuffer->QueryInterface(riid, ppSurface);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    if (ppOutput == nullptr)
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
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    *pDesc = m_desc;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    if (pStats == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    *pStats = m_stats;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    HRESULT hr = S_OK;
    
    if (pFullscreen != nullptr)
      *pFullscreen = !m_desc.Windowed;
    
    if ((ppTarget != nullptr) && !m_desc.Windowed)
      hr = this->GetContainingOutput(ppTarget);
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    if (pLastPresentCount == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    *pLastPresentCount = m_stats.PresentCount;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (Flags & DXGI_PRESENT_TEST)
      return S_OK;
    
    try {
      // Submit pending rendering commands
      // before recording the present code.
      m_presentDevice->FlushRenderingCommands();
      
      // Update swap chain properties. This will not only set
      // up vertical synchronization properly, but also apply
      // changes that were made to the window size even if the
      // Vulkan swap chain itself remains valid.
      DxvkSwapchainProperties swapchainProps;
      swapchainProps.preferredSurfaceFormat
        = m_presenter->pickSurfaceFormat(m_desc.BufferDesc.Format);
      swapchainProps.preferredPresentMode = SyncInterval == 0
        ? m_presenter->pickPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR)
        : m_presenter->pickPresentMode(VK_PRESENT_MODE_FIFO_KHR);
      swapchainProps.preferredBufferSize = GetWindowSize();
      
      m_presenter->recreateSwapchain(swapchainProps);
      m_presenter->presentImage();
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
    
    const VkExtent2D windowSize = GetWindowSize();
    
    m_desc.BufferDesc.Width  = Width  != 0 ? Width  : windowSize.width;
    m_desc.BufferDesc.Height = Height != 0 ? Height : windowSize.height;
    
    m_desc.Flags = SwapChainFlags;
    
    if (BufferCount != 0)
      m_desc.BufferCount = BufferCount;
    
    if (NewFormat != DXGI_FORMAT_UNKNOWN)
      m_desc.BufferDesc.Format = NewFormat;
    
    return CreateBackBuffer();
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    if (pNewTargetParameters == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    // TODO support fullscreen mode
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
    
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    
    if (Fullscreen)
      Logger::warn("DxgiSwapChain: Display mode changes not implemented");
    
    if (m_desc.Windowed && Fullscreen)
      return this->EnterFullscreenMode(pTarget);
    else if (!m_desc.Windowed && !Fullscreen)
      return this->LeaveFullscreenMode();
    
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::CreatePresenter() {
    try {
      m_presenter = new DxgiPresenter(
        m_device->GetDXVKDevice(),
        m_desc.OutputWindow);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT DxgiSwapChain::CreateBackBuffer() {
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    
    if (FAILED(GetSampleCount(m_desc.SampleDesc.Count, &sampleCount))) {
      Logger::err("DxgiSwapChain: Invalid sample count");
      return E_INVALIDARG;
    }
    
    if (FAILED(m_presentDevice->CreateSwapChainBackBuffer(&m_desc, &m_backBuffer))) {
      Logger::err("DxgiSwapChain: Failed to create back buffer");
      return E_FAIL;
    }
    
    try {
      m_presenter->updateBackBuffer(m_backBuffer->GetDXVKImage());
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  VkExtent2D DxgiSwapChain::GetWindowSize() const {
    RECT windowRect;
    ::GetClientRect(m_desc.OutputWindow, &windowRect);
    
    VkExtent2D result;
    result.width  = windowRect.right;
    result.height = windowRect.bottom;
    return result;
  }
  
  
  HRESULT DxgiSwapChain::EnterFullscreenMode(IDXGIOutput *pTarget) {
    Com<IDXGIOutput> output = pTarget;
    
    if (output == nullptr) {
      if (FAILED(GetContainingOutput(&output))) {
        Logger::err("DxgiSwapChain: Failed to enter fullscreen mode: Cannot query containing output");
        return E_FAIL;
      }
    }
    
    // Update swap chain description
    m_desc.Windowed = FALSE;
    
    // Change the window flags to remove the decoration etc.
    LONG style   = ::GetWindowLongW(m_desc.OutputWindow, GWL_STYLE);
    LONG exstyle = ::GetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE);
    
    m_windowState.style = style;
    m_windowState.exstyle = exstyle;
    ::GetWindowRect(m_desc.OutputWindow, &m_windowState.rect);
    
    style |= WS_POPUP | WS_SYSMENU;
    style &= ~(WS_CAPTION | WS_THICKFRAME);
    
    exstyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE);
    
    ::SetWindowLongW(m_desc.OutputWindow, GWL_STYLE, style);
    ::SetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE, exstyle);
    
    // Move the window so that it covers the entire output
    DXGI_OUTPUT_DESC desc;
    output->GetDesc(&desc);
    
    const RECT rect = desc.DesktopCoordinates;
    
    ::SetWindowPos(m_desc.OutputWindow, HWND_TOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::LeaveFullscreenMode() {
    m_desc.Windowed = TRUE;
    
    // FIXME wine only restores window flags if the application
    // has not modified them in the meantime. Some applications
    // may rely on that behaviour.
    const RECT rect = m_windowState.rect;
    
    ::SetWindowLongW(m_desc.OutputWindow, GWL_STYLE,   m_windowState.style);
    ::SetWindowLongW(m_desc.OutputWindow, GWL_EXSTYLE, m_windowState.exstyle);
    
    ::SetWindowPos(m_desc.OutputWindow, 0,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
    
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
  
}
