#include "dxgi_factory.h"
#include "dxgi_resource.h"
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
    
    // Create SDL window handle
    m_window = SDL_CreateWindowFrom(m_desc.OutputWindow);
    
    if (m_window == nullptr) {
      throw DxvkError(str::format(
        "DxgiSwapChain::DxgiSwapChain: Failed to create window:\n",
        SDL_GetError()));
    }
    
    // Adjust initial back buffer size. If zero, these
    // shall be set to the current window size.
    VkExtent2D windowSize = this->getWindowSize();
    
    if (m_desc.BufferDesc.Width  == 0) m_desc.BufferDesc.Width  = windowSize.width;
    if (m_desc.BufferDesc.Height == 0) m_desc.BufferDesc.Height = windowSize.height;
    
    // Set initial window mode and fullscreen state
    if (FAILED(this->SetFullscreenState(!pDesc->Windowed, nullptr)))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to set initial fullscreen state");
    
    this->createPresenter();
    this->createBackBuffer();
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    // We do not release the SDL window handle here since
    // that would destroy the underlying window as well.
  }
  
  
  HRESULT DxgiSwapChain::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIDeviceSubObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGISwapChain);
    
    Logger::warn("DxgiSwapChain::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT DxgiSwapChain::GetParent(REFIID riid, void** ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT DxgiSwapChain::GetDevice(REFIID riid, void** ppDevice) {
    return m_device->QueryInterface(riid, ppDevice);
  }
  
  
  HRESULT DxgiSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (Buffer > 0) {
      Logger::err("DxgiSwapChain::GetBuffer: Buffer > 0 not supported");
      return DXGI_ERROR_INVALID_CALL;
    }
    
    return m_backBufferIface->QueryInterface(riid, ppSurface);
  }
  
  
  HRESULT DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    if (ppOutput != nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // We can use the display index returned by SDL to query the
    // containing output, since DxgiAdapter::EnumOutputs uses the
    // same output IDs.
    std::lock_guard<std::mutex> lock(m_mutex);
    int32_t displayId = SDL_GetWindowDisplayIndex(m_window);
    
    if (displayId < 0) {
      Logger::err("DxgiSwapChain::GetContainingOutput: Failed to query window display index");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    return m_adapter->EnumOutputs(displayId, ppOutput);
  }
  
  
  HRESULT DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pDesc = m_desc;
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    if (pStats == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pStats = m_stats;
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::GetFullscreenState(
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
  
  
  HRESULT DxgiSwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    if (pLastPresentCount == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    *pLastPresentCount = m_stats.PresentCount;
    return S_OK;
  }
  
  
  HRESULT DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
      // Submit pending rendering commands
      // before recording the present code.
      m_presentDevice->FlushRenderingCommands();
    
      // TODO implement sync interval
      // TODO implement flags
      m_presenter->presentImage(m_backBufferView);
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT DxgiSwapChain::ResizeBuffers(
          UINT        BufferCount,
          UINT        Width,
          UINT        Height,
          DXGI_FORMAT NewFormat,
          UINT        SwapChainFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    VkExtent2D windowSize = this->getWindowSize();
    
    m_desc.BufferDesc.Width = Width != 0 ? Width : windowSize.width;
    m_desc.BufferDesc.Height = Height != 0 ? Height : windowSize.height;
    
    m_desc.Flags = SwapChainFlags;
    
    if (BufferCount != 0)
      m_desc.BufferCount = BufferCount;
    
    if (NewFormat != DXGI_FORMAT_UNKNOWN)
      m_desc.BufferDesc.Format = NewFormat;
    
    try {
      m_presenter->recreateSwapchain(
        m_desc.BufferDesc.Width,
        m_desc.BufferDesc.Height,
        m_desc.BufferDesc.Format);
      this->createBackBuffer();
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    if (pNewTargetParameters == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Applies to windowed mode
    SDL_SetWindowSize(m_window,
      pNewTargetParameters->Width,
      pNewTargetParameters->Height);
    
    // Applies to fullscreen mode
    SDL_DisplayMode displayMode;
    displayMode.format       = SDL_PIXELFORMAT_RGBA32;
    displayMode.w            = pNewTargetParameters->Width;
    displayMode.h            = pNewTargetParameters->Height;
    displayMode.refresh_rate = pNewTargetParameters->RefreshRate.Numerator
                             / pNewTargetParameters->RefreshRate.Denominator;
    displayMode.driverdata   = nullptr;
    
    // TODO test mode change flag
    
    if (SDL_SetWindowDisplayMode(m_window, &displayMode)) {
      Logger::err(str::format(
        "DxgiSwapChain::ResizeTarget: Failed to set display mode:\n",
        SDL_GetError()));
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    try {
      m_presenter->recreateSwapchain(
        m_desc.BufferDesc.Width,
        m_desc.BufferDesc.Height,
        m_desc.BufferDesc.Format);
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Unconditionally reset the swap chain to windowed mode first.
    // This required if the application wants to move the window to
    // a different display while remaining in fullscreen mode.
    if (SDL_SetWindowFullscreen(m_window, 0)) {
      Logger::err(str::format(
        "DxgiSwapChain::SetFullscreenState: Failed to set windowed mode:\n",
        SDL_GetError()));
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    
    m_desc.Windowed = !Fullscreen;
    
    if (Fullscreen) {
      // If a target output is specified, we need to move the
      // window to that output first while in windowed mode.
      if (pTarget != nullptr) {
        DXGI_OUTPUT_DESC outputDesc;
        
        if (FAILED(pTarget->GetDesc(&outputDesc))) {
          Logger::err("DxgiSwapChain::SetFullscreenState: Failed to query output properties");
          return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
        }
        
        SDL_SetWindowPosition(m_window,
          outputDesc.DesktopCoordinates.left,
          outputDesc.DesktopCoordinates.top);
      }
      
      // Now that the window is located at the target location,
      // SDL should fullscreen it on the requested display. We
      // only use borderless fullscreen for now, may be changed.
      if (SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP)) {
        Logger::err(str::format(
          "DxgiSwapChain::SetFullscreenState: Failed to set fullscreen mode:\n",
          SDL_GetError()));
        return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
      }
    }
    
    return S_OK;
  }
  
  
  void DxgiSwapChain::createPresenter() {
    m_presenter = new DxgiPresenter(
      m_device->GetDXVKDevice(),
      m_desc.OutputWindow,
      m_desc.BufferDesc.Width,
      m_desc.BufferDesc.Height,
      m_desc.BufferDesc.Format);
  }
  
  
  void DxgiSwapChain::createBackBuffer() {
    // Pick the back buffer format based on the requested swap chain format
    DxgiFormatPair bufferFormat = m_adapter->LookupFormat(m_desc.BufferDesc.Format);
    Logger::info(str::format("DxgiSwapChain: Creating back buffer with ", bufferFormat.actual));
    
    // TODO support proper multi-sampling
    const Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();
    
    // Create an image that can be rendered to
    // and that can be used as a sampled texture.
    Com<IDXGIImageResourcePrivate> resource;
    
    DxvkImageCreateInfo imageInfo;
    imageInfo.type          = VK_IMAGE_TYPE_2D;
    imageInfo.format        = bufferFormat.actual;
    imageInfo.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    imageInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent.width  = m_desc.BufferDesc.Width;
    imageInfo.extent.height = m_desc.BufferDesc.Height;
    imageInfo.extent.depth  = 1;
    imageInfo.numLayers     = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.stages        = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                            | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_TRANSFER_BIT;
    imageInfo.access        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_TRANSFER_WRITE_BIT
                            | VK_ACCESS_TRANSFER_READ_BIT
                            | VK_ACCESS_SHADER_READ_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    if (dxvkDevice->features().geometryShader)
      imageInfo.stages      |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    
    if (dxvkDevice->features().tessellationShader) {
      imageInfo.stages      |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                            |  VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }
    
    if (FAILED(DXGICreateImageResourcePrivate(m_device.ptr(), &imageInfo,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DXGI_USAGE_BACK_BUFFER | m_desc.BufferUsage,
          &resource)))
      throw DxvkError("DxgiSwapChain::createBackBuffer: Failed to create back buffer");
      
    m_backBuffer = resource->GetDXVKImage();
    
    // Create an image view that allows the
    // image to be bound as a shader resource.
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format     = imageInfo.format;
    viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel   = 0;
    viewInfo.numLevels  = 1;
    viewInfo.minLayer   = 0;
    viewInfo.numLayers  = 1;
    
    m_backBufferView = dxvkDevice->createImageView(m_backBuffer, viewInfo);
    
    // Wrap the back buffer image into an interface
    // that the device can use to access the image.
    if (FAILED(m_presentDevice->WrapSwapChainBackBuffer(resource.ptr(), &m_desc, &m_backBufferIface)))
      throw DxvkError("DxgiSwapChain::createBackBuffer: Failed to create back buffer interface");
    
    // Initialize the image properly so that
    // it can be used in a DXVK context
    m_presenter->initBackBuffer(m_backBuffer);
  }
  
  
  VkExtent2D DxgiSwapChain::getWindowSize() const {
    int winWidth = 0;
    int winHeight = 0;
    
    SDL_GetWindowSize(m_window, &winWidth, &winHeight);
    
    VkExtent2D result;
    result.width  = winWidth;
    result.height = winHeight;
    return result;
  }
  
}
