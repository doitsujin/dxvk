#include <dxvk_swapchain.h>

#include "dxgi_factory.h"
#include "dxgi_swapchain.h"

namespace dxvk {
  
  DxgiSwapChain::DxgiSwapChain(
          DxgiFactory*          factory,
          IUnknown*             pDevice,
          DXGI_SWAP_CHAIN_DESC* pDesc) {
    TRACE(this, factory, pDevice);
  }
  
  
  DxgiSwapChain::~DxgiSwapChain() {
    TRACE(this);
  }
  
  
  HRESULT DxgiSwapChain::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IDXGISwapChain);
    
    Logger::warn("DxgiSwapChain::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT DxgiSwapChain::GetParent(REFIID riid, void** ppParent) {
    Logger::err("DxgiSwapChain::GetParent: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetDevice(REFIID riid, void** ppDevice) {
    Logger::err("DxgiSwapChain::GetDevice: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    Logger::err("DxgiSwapChain::GetBuffer: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    Logger::err("DxgiSwapChain::GetContainingOutput: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    Logger::err("DxgiSwapChain::GetDesc: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    Logger::err("DxgiSwapChain::GetFrameStatistics: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetFullscreenState(
          BOOL*         pFullscreen,
          IDXGIOutput** ppTarget) {
    Logger::err("DxgiSwapChain::GetFullscreenState: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    Logger::err("DxgiSwapChain::GetLastPresentCount: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::Present(UINT SyncInterval, UINT Flags) {
    Logger::err("DxgiSwapChain::Present: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::ResizeBuffers(
          UINT        BufferCount,
          UINT        Width,
          UINT        Height,
          DXGI_FORMAT NewFormat,
          UINT        SwapChainFlags) {
    Logger::err("DxgiSwapChain::ResizeBuffers: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    Logger::err("DxgiSwapChain::ResizeTarget: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT DxgiSwapChain::SetFullscreenState(
          BOOL          Fullscreen,
          IDXGIOutput*  pTarget) {
    Logger::err("DxgiSwapChain::SetFullscreenState: Not implemented");
    return E_NOTIMPL;
  }
  
}
