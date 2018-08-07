#pragma once

#include <vector>

#include <dxvk_instance.h>

#include "dxgi_adapter.h"
#include "dxgi_options.h"

namespace dxvk {
    
  class DxgiFactory : public DxgiObject<IDXGIFactory2> {
    
  public:
    
    DxgiFactory();
    ~DxgiFactory();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                riid,
            void**                ppParent) final;
    
    BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled() final;

    HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(
            HMODULE               Module,
            IDXGIAdapter**        ppAdapter) final;
    
    HRESULT STDMETHODCALLTYPE CreateSwapChain(
            IUnknown*             pDevice,
            DXGI_SWAP_CHAIN_DESC* pDesc,
            IDXGISwapChain**      ppSwapChain) final;
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(
            IUnknown*             pDevice,
            HWND                  hWnd,
      const DXGI_SWAP_CHAIN_DESC1* pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
            IDXGIOutput*          pRestrictToOutput,
            IDXGISwapChain1**     ppSwapChain) final;

    HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(
            IUnknown*             pDevice,
            IUnknown*             pWindow,
      const DXGI_SWAP_CHAIN_DESC1* pDesc,
            IDXGIOutput*          pRestrictToOutput,
            IDXGISwapChain1**     ppSwapChain) final;
    
    HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(
            IUnknown*             pDevice,
      const DXGI_SWAP_CHAIN_DESC1* pDesc,
            IDXGIOutput*          pRestrictToOutput,
            IDXGISwapChain1**     ppSwapChain) final;
    
    HRESULT STDMETHODCALLTYPE EnumAdapters(
            UINT                  Adapter,
            IDXGIAdapter**        ppAdapter) final;
    
    HRESULT STDMETHODCALLTYPE EnumAdapters1(
            UINT                  Adapter,
            IDXGIAdapter1**       ppAdapter) final;
    
    HRESULT STDMETHODCALLTYPE GetWindowAssociation(
            HWND*                 pWindowHandle) final;
    
    HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(
            HANDLE                hResource,
            LUID*                 pLuid) final;

    HRESULT STDMETHODCALLTYPE MakeWindowAssociation(
            HWND                  WindowHandle,
            UINT                  Flags) final;
    
    BOOL STDMETHODCALLTYPE IsCurrent() final;
    
    HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(
            HWND                  WindowHandle,
            UINT                  wMsg,
            DWORD*                pdwCookie) final;
    
    HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(
            HANDLE                hEvent,
            DWORD*                pdwCookie) final;
    
    HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(
            HWND                  WindowHandle,
            UINT                  wMsg,
            DWORD*                pdwCookie) final;

    HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(
            HANDLE                hEvent,
            DWORD*                pdwCookie) final;

    void STDMETHODCALLTYPE UnregisterStereoStatus(
            DWORD                 dwCookie) final;

    void STDMETHODCALLTYPE UnregisterOcclusionStatus(
            DWORD                 dwCookie) final;
    
    const DxgiOptions* GetOptions() const {
      return &m_options;
    }
    
  private:
    
    Rc<DxvkInstance> m_instance;
    DxgiOptions      m_options;
    
    HWND m_associatedWindow = nullptr;
    
  };
  
}
