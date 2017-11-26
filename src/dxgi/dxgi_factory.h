#pragma once

#include <vector>

#include <dxvk_instance.h>

#include "dxgi_adapter.h"

namespace dxvk {
    
  class DxgiFactory : public DxgiObject<IDXGIFactory1> {
    
  public:
    
    DxgiFactory();
    ~DxgiFactory();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    HRESULT GetParent(
            REFIID  riid,
            void**  ppParent) final;
    
    HRESULT CreateSoftwareAdapter(
            HMODULE         Module,
            IDXGIAdapter**  ppAdapter) final;
    
    HRESULT CreateSwapChain(
            IUnknown*             pDevice,
            DXGI_SWAP_CHAIN_DESC* pDesc,
            IDXGISwapChain**      ppSwapChain) final;
    
    HRESULT EnumAdapters(
            UINT            Adapter,
            IDXGIAdapter**  ppAdapter) final;
    
    HRESULT EnumAdapters1(
            UINT            Adapter,
            IDXGIAdapter1** ppAdapter) final;
    
    HRESULT GetWindowAssociation(
            HWND *pWindowHandle) final;
    
    HRESULT MakeWindowAssociation(
            HWND WindowHandle,
            UINT Flags) final;
    
    BOOL IsCurrent();
    
  private:
    
    Rc<DxvkInstance>              m_instance;
    std::vector<Rc<DxvkAdapter>>  m_adapters;
    
    HWND m_associatedWindow = nullptr;
    
  };
  
}
