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
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevicePrivate),
        reinterpret_cast<void**>(&m_device))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    // Retrieve the adapter, which is going
    // to be used to enumerate displays.
    Com<IDXGIDevice> device;
    
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIDevice),
        reinterpret_cast<void**>(&device))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
    if (FAILED(device->GetAdapter(&m_adapter)))
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
    
    // Set initial window mode and fullscreen state
    if (FAILED(this->ResizeTarget(&pDesc->BufferDesc)))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to set initial display mode");
    
    if (FAILED(this->SetFullscreenState(!pDesc->Windowed, nullptr)))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Failed to set initial fullscreen state");
    
    // TODO clean up here
    Rc<DxvkDevice>  dxvkDevice  = m_device->GetDXVKDevice();
    Rc<DxvkAdapter> dxvkAdapter = dxvkDevice->adapter();
    
    m_context = dxvkDevice->createContext();
    m_commandList = dxvkDevice->createCommandList();
    
    m_acquireSync = dxvkDevice->createSemaphore();
    m_presentSync = dxvkDevice->createSemaphore();
    
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(m_desc.OutputWindow, GWLP_HINSTANCE));
    
    m_surface = dxvkAdapter->createSurface(
      instance, m_desc.OutputWindow);
    
    DxvkSwapchainProperties swapchainProperties;
    swapchainProperties.preferredSurfaceFormat.format     = VK_FORMAT_B8G8R8A8_SNORM;
    swapchainProperties.preferredSurfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainProperties.preferredPresentMode              = VK_PRESENT_MODE_FIFO_KHR;
    swapchainProperties.preferredBufferSize.width         = m_desc.BufferDesc.Width;
    swapchainProperties.preferredBufferSize.height        = m_desc.BufferDesc.Height;
    
    m_swapchain = dxvkDevice->createSwapchain(
      m_surface, swapchainProperties);
    
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
    
    // TODO implement generic swap chain client interface
    Com<ID3D11DevicePrivate> d3d11device;
    
    if (FAILED(m_device->QueryInterface(__uuidof(ID3D11DevicePrivate),
          reinterpret_cast<void**>(&d3d11device)))) {
      Logger::err("DxgiSwapChain::Present: Invalid swap chain client interface");
      return E_INVALIDARG;
    }
    
    try {
      // Submit pending rendering commands
      // before recording the present code.
      d3d11device->FlushRenderingCommands();
    
      // TODO implement sync interval
      // TODO implement flags
      
      auto dxvkDevice = d3d11device->GetDXVKDevice();
      
      auto framebuffer = m_swapchain->getFramebuffer(m_acquireSync);
      auto framebufferSize = framebuffer->size();
      
      m_context->beginRecording(m_commandList);
      m_context->bindFramebuffer(framebuffer);
      
      // TODO render back buffer into the swap image,
      // the clear operation is only a placeholder.
      VkClearAttachment clearAttachment;
      clearAttachment.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
      clearAttachment.colorAttachment = 0;
      clearAttachment.clearValue.color.float32[0] = 1.0f;
      clearAttachment.clearValue.color.float32[1] = 1.0f;
      clearAttachment.clearValue.color.float32[2] = 1.0f;
      clearAttachment.clearValue.color.float32[3] = 1.0f;
      
      VkClearRect clearArea;
      clearArea.rect           = VkRect2D { { 0, 0 }, framebufferSize.width, framebufferSize.height };
      clearArea.baseArrayLayer = 0;
      clearArea.layerCount     = framebufferSize.layers;
      
      m_context->clearRenderTarget(
        clearAttachment,
        clearArea);
      
      m_context->endRecording();
      
      dxvkDevice->submitCommandList(m_commandList,
        m_acquireSync, m_presentSync);
      
      m_swapchain->present(m_presentSync);
      
      // FIXME Make sure that the semaphores and the command
      // list can be safely used without stalling the device.
      dxvkDevice->waitForIdle();
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
    
    m_desc.BufferDesc.Width  = Width;
    m_desc.BufferDesc.Height = Height;
    m_desc.BufferDesc.Format = NewFormat;
    m_desc.Flags             = SwapChainFlags;
    
    if (BufferCount != 0)
      m_desc.BufferCount     = BufferCount;
    
    try {
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
    
    if (SDL_SetWindowDisplayMode(m_window, &displayMode)) {
      throw DxvkError(str::format(
        "DxgiSwapChain::ResizeTarget: Failed to set display mode:\n",
        SDL_GetError()));
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    return S_OK;
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
  
  
  void DxgiSwapChain::createBackBuffer() {
    // TODO select format based on DXGI format
    // TODO support proper multi-sampling
    const Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();
    
    // TODO implement generic swap chain client interface
    Com<ID3D11DevicePrivate> d3d11device;
    
    if (FAILED(m_device->QueryInterface(__uuidof(ID3D11DevicePrivate),
          reinterpret_cast<void**>(&d3d11device))))
      throw DxvkError("DxgiSwapChain::Present: Invalid swap chain client interface");
    
    // Create an image that can be rendered to
    // and that can be used as a sampled texture.
    Com<IDXGIImageResourcePrivate> resource;
    
    DxvkImageCreateInfo imageInfo;
    imageInfo.type          = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SNORM;
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
                            | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                            | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                            | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                            | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_TRANSFER_BIT;
    imageInfo.access        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_TRANSFER_WRITE_BIT
                            | VK_ACCESS_TRANSFER_READ_BIT
                            | VK_ACCESS_SHADER_READ_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    
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
    if (FAILED(d3d11device->WrapSwapChainBackBuffer(resource.ptr(), &m_desc, &m_backBufferIface)))
      throw DxvkError("DxgiSwapChain::createBackBuffer: Failed to create back buffer interface");
  }
  
}
