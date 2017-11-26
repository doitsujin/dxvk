#include <dxvk_swapchain.h>

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
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXVKDevice),
        reinterpret_cast<void**>(&m_device))))
      throw DxvkError("DxgiSwapChain::DxgiSwapChain: Invalid device");
    
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
    
    Logger::err("DxgiSwapChain::GetBuffer: Not implemented");
    return E_NOTIMPL;
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
    
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(m_device->GetAdapter(&adapter)))
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    
    return adapter->EnumOutputs(displayId, ppOutput);
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
    
    Logger::err("DxgiSwapChain::Present: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::ResizeBuffers(
          UINT        BufferCount,
          UINT        Width,
          UINT        Height,
          DXGI_FORMAT NewFormat,
          UINT        SwapChainFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Logger::err("DxgiSwapChain::ResizeBuffers: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    if (pNewTargetParameters == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_desc.BufferDesc = *pNewTargetParameters;
    
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
  
}
