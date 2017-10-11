#include "dxgi_factory.h"
#include "dxgi_swapchain.h"

namespace dxvk {
  
  DxgiFactory::DxgiFactory()
  : m_instance(new DxvkInstance()) {
    TRACE(this);
    
    auto adapters = m_instance->enumAdapters();
    for (auto a : adapters)
      m_adapters.push_back(new DxgiAdapter(this, a));
  }
  
  
  DxgiFactory::~DxgiFactory() {
    TRACE(this);
  }
  
  
  HRESULT DxgiFactory::QueryInterface(
          REFIID  riid,
          void**  ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IDXGIFactory);
    
    Logger::warn("DxgiFactory::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT DxgiFactory::GetParent(
          REFIID  riid,
          void**  ppParent) {
    Logger::warn("DxgiFactory::GetParent: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT DxgiFactory::CreateSoftwareAdapter(
          HMODULE         Module,
          IDXGIAdapter**  ppAdapter) {
    Logger::err("DxgiFactory::CreateSoftwareAdapter: Software adapters not supported");
    return DXGI_ERROR_UNSUPPORTED;
  }
  
  
  HRESULT DxgiFactory::CreateSwapChain(
          IUnknown*             pDevice,
          DXGI_SWAP_CHAIN_DESC* pDesc,
          IDXGISwapChain**      ppSwapChain) {
    TRACE(this, pDevice, pDesc, ppSwapChain);
    return DXGI_ERROR_UNSUPPORTED;
  }
  
  
  HRESULT DxgiFactory::EnumAdapters(
          UINT            Adapter,
          IDXGIAdapter**  ppAdapter) {
    TRACE(this, Adapter, ppAdapter);
    
    if (ppAdapter == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    if (Adapter >= m_adapters.size())
      return DXGI_ERROR_NOT_FOUND;
    
    *ppAdapter = m_adapters.at(Adapter).ref();
    return S_OK;
  }
  
  
  HRESULT DxgiFactory::GetWindowAssociation(HWND *pWindowHandle) {
    if (pWindowHandle == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    *pWindowHandle = m_associatedWindow;
    return S_OK;
  }
  
  
  HRESULT DxgiFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags) {
    TRACE(this, WindowHandle, Flags);
    Logger::warn("DxgiFactory::MakeWindowAssociation: Ignoring flags");
    m_associatedWindow = WindowHandle;
    return S_OK;
  }
  
}
