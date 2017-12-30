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
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevicePrivate),
        reinterpret_cast<void**>(&m_device))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    if (FAILED(m_device->GetAdapter(&adapter))
     || FAILED(adapter->QueryInterface(__uuidof(IDXGIAdapterPrivate),
        reinterpret_cast<void**>(&m_adapter))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to retrieve adapter");
    
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
//     if (FAILED(this->SetFullscreenState(!pDesc->Windowed, nullptr)))
//       throw DxvkError("DxgiSwapChain: Failed to set initial fullscreen state");
    
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
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetParent(REFIID riid, void** ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDevice(REFIID riid, void** ppDevice) {
    return m_device->QueryInterface(riid, ppDevice);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (Buffer > 0) {
      Logger::err("DxgiSwapChain::GetBuffer: Buffer > 0 not supported");
      return DXGI_ERROR_INVALID_CALL;
    }
    
    return m_backBuffer->QueryInterface(riid, ppSurface);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    if (ppOutput != nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    Logger::err("DxgiSwapChain::GetContainingOutput: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pDesc = m_desc;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    if (pStats == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pStats = m_stats;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pLastPresentCount = m_stats.PresentCount;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
        : m_presenter->pickPresentMode(VK_PRESENT_MODE_MAILBOX_KHR);
      swapchainProps.preferredBufferSize = GetWindowSize();
      
      m_presenter->recreateSwapchain(swapchainProps);
      
      for (uint32_t i = 0; i < SyncInterval || i < 1; i++)
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
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::err("DxgiSwapChain::SetFullscreenState: Not implemented");
    return E_NOTIMPL;
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
