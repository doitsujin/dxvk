#pragma once

#include <vector>

#include <dxvk_instance.h>

#include "dxgi_adapter.h"

namespace dxvk {
    
  class DxgiFactory : public DxgiObject<IDXGIFactory1> {
    
  public:
    
    DxgiFactory();
    ~DxgiFactory();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID  riid,
            void**  ppParent) final;
    
    HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(
            HMODULE         Module,
            IDXGIAdapter**  ppAdapter) final;
    
    HRESULT STDMETHODCALLTYPE CreateSwapChain(
            IUnknown*             pDevice,
            DXGI_SWAP_CHAIN_DESC* pDesc,
            IDXGISwapChain**      ppSwapChain) final;
    
    HRESULT STDMETHODCALLTYPE EnumAdapters(
            UINT            Adapter,
            IDXGIAdapter**  ppAdapter) final;
    
    HRESULT STDMETHODCALLTYPE EnumAdapters1(
            UINT            Adapter,
            IDXGIAdapter1** ppAdapter) final;
    
    HRESULT STDMETHODCALLTYPE GetWindowAssociation(
            HWND *pWindowHandle) final;
    
    HRESULT STDMETHODCALLTYPE MakeWindowAssociation(
            HWND WindowHandle,
            UINT Flags) final;
    
    BOOL STDMETHODCALLTYPE IsCurrent();
    
  private:
    
    Rc<DxvkInstance>              m_instance;
    std::vector<Rc<DxvkAdapter>>  m_adapters;
    
    HWND m_associatedWindow = nullptr;
    
  };
  
}
