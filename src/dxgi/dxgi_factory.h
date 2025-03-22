#pragma once

#include <vector>
#include <mutex>

#include "dxgi_adapter.h"
#include "dxgi_monitor.h"
#include "dxgi_options.h"

#include "../dxvk/dxvk_instance.h"

namespace dxvk {

  class DxgiFactory;

  struct DXVK_VK_GLOBAL_HDR_STATE {
    uint32_t Serial;
    DXGI_COLOR_SPACE_TYPE ColorSpace;
    DXGI_VK_HDR_METADATA  Metadata;
  };

  class DxgiVkFactory : public IDXGIVkInteropFactory1 {

  public:

    DxgiVkFactory(DxgiFactory* pFactory);

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);

    void STDMETHODCALLTYPE GetVulkanInstance(
            VkInstance*               pInstance,
            PFN_vkGetInstanceProcAddr* ppfnVkGetInstanceProcAddr);

    HRESULT STDMETHODCALLTYPE GetGlobalHDRState(
            DXGI_COLOR_SPACE_TYPE   *pOutColorSpace,
            DXGI_HDR_METADATA_HDR10 *pOutMetadata);

    HRESULT STDMETHODCALLTYPE SetGlobalHDRState(
            DXGI_COLOR_SPACE_TYPE    ColorSpace,
      const DXGI_HDR_METADATA_HDR10 *pMetadata);

  private:

    DxgiFactory* m_factory;

  };


  class DxgiFactory : public DxgiObject<IDXGIFactory7> {
    
  public:
    
    DxgiFactory(UINT Flags);
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
    
    HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(
            LUID                  AdapterLuid,
            REFIID                riid,
            void**                ppvAdapter) final;
    
    HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(
            UINT                  Adapter,
            DXGI_GPU_PREFERENCE   GpuPreference,
            REFIID                riid,
            void**                ppvAdapter);

    HRESULT STDMETHODCALLTYPE EnumWarpAdapter(
            REFIID                riid,
            void**                ppvAdapter) final;

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
    
    UINT STDMETHODCALLTYPE GetCreationFlags() final;

    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
            DXGI_FEATURE          Feature,
            void*                 pFeatureSupportData,
            UINT                  FeatureSupportDataSize) final;

    HRESULT STDMETHODCALLTYPE RegisterAdaptersChangedEvent(
            HANDLE                hEvent,
            DWORD*                pdwCookie);

    HRESULT STDMETHODCALLTYPE UnregisterAdaptersChangedEvent(
            DWORD                 Cookie);

    BOOL UseMonitorFallback() const {
      return m_monitorFallback;
    }

    Rc<DxvkInstance> GetDXVKInstance() const {
      return m_instance;
    }

    const DxgiOptions* GetOptions() const {
      return &m_options;
    }

    DxgiMonitorInfo* GetMonitorInfo() {
      return &m_monitorInfo;
    }

    DXVK_VK_GLOBAL_HDR_STATE GlobalHDRState();
    
  private:
    
    Rc<DxvkInstance> m_instance;
    DxgiVkFactory    m_interop;
    DxgiOptions      m_options;
    DxgiMonitorInfo  m_monitorInfo;
    UINT             m_flags;
    BOOL             m_monitorFallback;
      

    HRESULT STDMETHODCALLTYPE CreateSwapChainBase(
            IUnknown*             pDevice,
            HWND                  hWnd,
      const DXGI_SWAP_CHAIN_DESC1* pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
            IDXGIOutput*          pRestrictToOutput,
            IDXGISwapChain1**     ppSwapChain);
  };
  
}
