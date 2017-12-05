#pragma once

#include <memory>
#include <mutex>

#include <dxvk_surface.h>
#include <dxvk_swapchain.h>

#include "dxgi_interfaces.h"
#include "dxgi_object.h"
#include "dxgi_presenter.h"

#include "../d3d11/d3d11_interfaces.h"

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  class DxgiFactory;
  
  class DxgiSwapChain : public DxgiObject<IDXGISwapChain> {
    
  public:
    
    DxgiSwapChain(
          DxgiFactory*          factory,
          IUnknown*             pDevice,
          DXGI_SWAP_CHAIN_DESC* pDesc);
    ~DxgiSwapChain();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
            
    HRESULT GetParent(
            REFIID  riid,
            void**  ppParent) final;
    
    HRESULT GetDevice(
            REFIID  riid,
            void**  ppDevice) final;
    
    HRESULT GetBuffer(
            UINT    Buffer,
            REFIID  riid,
            void**  ppSurface) final;
    
    HRESULT GetContainingOutput(
            IDXGIOutput **ppOutput) final;
    
    HRESULT GetDesc(
            DXGI_SWAP_CHAIN_DESC *pDesc) final;
    
    HRESULT GetFrameStatistics(
            DXGI_FRAME_STATISTICS *pStats) final;
    
    HRESULT GetFullscreenState(
            BOOL        *pFullscreen,
            IDXGIOutput **ppTarget) final;
    
    HRESULT GetLastPresentCount(
            UINT *pLastPresentCount) final;
    
    HRESULT Present(
            UINT SyncInterval,
            UINT Flags) final;
    
    HRESULT ResizeBuffers(
            UINT        BufferCount,
            UINT        Width,
            UINT        Height,
            DXGI_FORMAT NewFormat,
            UINT        SwapChainFlags) final;
    
    HRESULT ResizeTarget(
      const DXGI_MODE_DESC *pNewTargetParameters) final;
    
    HRESULT SetFullscreenState(
            BOOL        Fullscreen,
            IDXGIOutput *pTarget) final;

  private:
    
    std::mutex m_mutex;
    
    Com<DxgiFactory>                m_factory;
    Com<IDXGIAdapterPrivate>        m_adapter;
    Com<IDXGIDevicePrivate>         m_device;
    Com<IDXGIPresentDevicePrivate>  m_presentDevice;
    
    DXGI_SWAP_CHAIN_DESC  m_desc;
    DXGI_FRAME_STATISTICS m_stats;
    
    Rc<DxvkContext>     m_context;
    Rc<DxvkCommandList> m_commandList;
    Rc<DxvkSurface>     m_surface;
    Rc<DxvkSwapchain>   m_swapchain;
    
    Rc<DxvkSemaphore>   m_acquireSync;
    Rc<DxvkSemaphore>   m_presentSync;
    
    Rc<DxgiPresenter>   m_presenter;
    
    Rc<DxvkImage>       m_backBuffer;
    Rc<DxvkImageView>   m_backBufferView;
    Com<IUnknown>       m_backBufferIface;
    
    void createPresenter();
    void createBackBuffer();
    
    void createContext();
    
    VkExtent2D getWindowSize() const;
        
  };
  
}
