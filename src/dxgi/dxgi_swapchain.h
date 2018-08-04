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
  
  class DxgiDevice;
  class DxgiFactory;
  class DxgiOutput;
  
  class DxgiSwapChain : public DxgiObject<IDXGISwapChain1> {
    
  public:
    
    DxgiSwapChain(
            DxgiFactory*                pFactory,
            IUnknown*                   pDevice,
            HWND                        hWnd,
      const DXGI_SWAP_CHAIN_DESC1*      pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*  pFullscreenDesc);
    
    ~DxgiSwapChain();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject) final;
            
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                    riid,
            void**                    ppParent) final;
    
    HRESULT STDMETHODCALLTYPE GetDevice(
            REFIID                    riid,
            void**                    ppDevice) final;
    
    HRESULT STDMETHODCALLTYPE GetBuffer(
            UINT                      Buffer,
            REFIID                    riid,
            void**                    ppSurface) final;
    
    HRESULT STDMETHODCALLTYPE GetContainingOutput(
            IDXGIOutput**             ppOutput) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_SWAP_CHAIN_DESC*     pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc1(
            DXGI_SWAP_CHAIN_DESC1*    pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetFullscreenState(
            BOOL*                     pFullscreen,
            IDXGIOutput**             ppTarget) final;
    
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
            DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) final;

    HRESULT STDMETHODCALLTYPE GetHwnd(
            HWND*                     pHwnd) final;

    HRESULT STDMETHODCALLTYPE GetCoreWindow(
            REFIID                    refiid,
            void**                    ppUnk) final;

    HRESULT STDMETHODCALLTYPE GetBackgroundColor(
            DXGI_RGBA*                pColor) final;
    
    HRESULT STDMETHODCALLTYPE GetRotation(
            DXGI_MODE_ROTATION*       pRotation) final;
    
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
            IDXGIOutput**             ppRestrictToOutput) final;
    
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
            DXGI_FRAME_STATISTICS*    pStats) final;
    
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(
            UINT*                     pLastPresentCount) final;
    
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() final;

    HRESULT STDMETHODCALLTYPE Present(
            UINT                      SyncInterval,
            UINT                      Flags) final;
    
    HRESULT STDMETHODCALLTYPE Present1(
            UINT                      SyncInterval,
            UINT                      PresentFlags,
      const DXGI_PRESENT_PARAMETERS*  pPresentParameters) final;

    HRESULT STDMETHODCALLTYPE ResizeBuffers(
            UINT                      BufferCount,
            UINT                      Width,
            UINT                      Height,
            DXGI_FORMAT               NewFormat,
            UINT                      SwapChainFlags) final;
    
    HRESULT STDMETHODCALLTYPE ResizeTarget(
      const DXGI_MODE_DESC*           pNewTargetParameters) final;
    
    HRESULT STDMETHODCALLTYPE SetFullscreenState(
            BOOL                      Fullscreen,
            IDXGIOutput*              pTarget) final;
    
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(
      const DXGI_RGBA*                pColor) final;

    HRESULT STDMETHODCALLTYPE SetRotation(
            DXGI_MODE_ROTATION        Rotation) final;

    HRESULT SetGammaControl(
      const DXGI_GAMMA_CONTROL*       pGammaControl);
    
    HRESULT SetDefaultGammaControl();
    
  private:
    
    struct WindowState {
      LONG style   = 0;
      LONG exstyle = 0;
      RECT rect    = { 0, 0, 0, 0 };
    };
    
    std::mutex                      m_lockWindow;
    std::mutex                      m_lockBuffer;

    Com<DxgiFactory>                m_factory;
    Com<DxgiAdapter>                m_adapter;
    Com<DxgiDevice>                 m_device;
    Com<IDXGIVkPresenter>           m_presentDevice;
    
    HWND                            m_window;
    DXGI_SWAP_CHAIN_DESC1           m_desc;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_descFs;
    DXGI_FRAME_STATISTICS           m_stats;
    
    Rc<DxgiVkPresenter>             m_presenter;
    Com<IDXGIVkBackBuffer>          m_backBuffer;
    
    HMONITOR                        m_monitor;
    WindowState                     m_windowState;
    
    HRESULT CreatePresenter();
    HRESULT CreateBackBuffer();
    
    VkExtent2D GetWindowSize() const;
    
    HRESULT EnterFullscreenMode(
            IDXGIOutput             *pTarget);
    
    HRESULT LeaveFullscreenMode();
    
    HRESULT ChangeDisplayMode(
            IDXGIOutput*            pOutput,
      const DXGI_MODE_DESC*         pDisplayMode);
    
    HRESULT RestoreDisplayMode(
            IDXGIOutput*            pOutput);
    
    HRESULT GetSampleCount(
            UINT                    Count,
            VkSampleCountFlagBits*  pCount) const;
    
  };
  
}
