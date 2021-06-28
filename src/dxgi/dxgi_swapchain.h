#pragma once

#include <memory>
#include <mutex>

#include "dxgi_interfaces.h"
#include "dxgi_monitor.h"
#include "dxgi_object.h"

#include "../d3d11/d3d11_interfaces.h"

#include "../spirv/spirv_module.h"

#include "../util/util_time.h"

namespace dxvk {
  
  class DxgiDevice;
  class DxgiFactory;
  class DxgiOutput;
  
  class DxgiSwapChain : public DxgiObject<IDXGISwapChain4> {
    
  public:
    
    DxgiSwapChain(
            IDXGIFactory*               pFactory,
            IDXGIVkSwapChain*           pPresenter,
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
    
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() final;

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
    
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(
            UINT                      BufferCount,
            UINT                      Width,
            UINT                      Height,
            DXGI_FORMAT               Format,
            UINT                      SwapChainFlags,
      const UINT*                     pCreationNodeMask,
            IUnknown* const*          ppPresentQueue) final;

    HRESULT STDMETHODCALLTYPE ResizeTarget(
      const DXGI_MODE_DESC*           pNewTargetParameters) final;
    
    HRESULT STDMETHODCALLTYPE SetFullscreenState(
            BOOL                      Fullscreen,
            IDXGIOutput*              pTarget) final;
    
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(
      const DXGI_RGBA*                pColor) final;

    HRESULT STDMETHODCALLTYPE SetRotation(
            DXGI_MODE_ROTATION        Rotation) final;
    
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() final;

    HRESULT STDMETHODCALLTYPE GetMatrixTransform(
            DXGI_MATRIX_3X2_F*        pMatrix) final;
    
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
            UINT*                     pMaxLatency) final;
    
    HRESULT STDMETHODCALLTYPE GetSourceSize(
            UINT*                     pWidth,
            UINT*                     pHeight) final;
    
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(
      const DXGI_MATRIX_3X2_F*        pMatrix) final;
    
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
            UINT                      MaxLatency) final;

    HRESULT STDMETHODCALLTYPE SetSourceSize(
            UINT                      Width,
            UINT                      Height) final;
    
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(
            DXGI_COLOR_SPACE_TYPE     ColorSpace,
            UINT*                     pColorSpaceSupport) final;

    HRESULT STDMETHODCALLTYPE SetColorSpace1(
            DXGI_COLOR_SPACE_TYPE     ColorSpace) final;
    
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(
            DXGI_HDR_METADATA_TYPE    Type,
            UINT                      Size,
            void*                     pMetaData) final;

    HRESULT STDMETHODCALLTYPE SetGammaControl(
            UINT                      NumPoints,
      const DXGI_RGB*                 pGammaCurve);
    
  private:
    
    struct WindowState {
      LONG style   = 0;
      LONG exstyle = 0;
      RECT rect    = { 0, 0, 0, 0 };
    };
    
    dxvk::recursive_mutex           m_lockWindow;
    dxvk::mutex                     m_lockBuffer;

    Com<IDXGIFactory>               m_factory;
    Com<IDXGIAdapter>               m_adapter;
    Com<IDXGIOutput>                m_target;
    Com<IDXGIVkMonitorInfo>         m_monitorInfo;
    
    HWND                            m_window;
    DXGI_SWAP_CHAIN_DESC1           m_desc;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_descFs;
    UINT                            m_presentCount;

    Com<IDXGIVkSwapChain>           m_presenter;
    
    HMONITOR                        m_monitor;
    WindowState                     m_windowState;
    
    HRESULT EnterFullscreenMode(
            IDXGIOutput             *pTarget);
    
    HRESULT LeaveFullscreenMode();
    
    HRESULT ChangeDisplayMode(
            IDXGIOutput*            pOutput,
      const DXGI_MODE_DESC*         pDisplayMode);
    
    HRESULT RestoreDisplayMode(
            HMONITOR                hMonitor);
    
    HRESULT GetSampleCount(
            UINT                    Count,
            VkSampleCountFlagBits*  pCount) const;
    
    HRESULT GetOutputFromMonitor(
            HMONITOR                Monitor,
            IDXGIOutput**           ppOutput);
    
    HRESULT AcquireMonitorData(
            HMONITOR                hMonitor,
            DXGI_VK_MONITOR_DATA**  ppData);
    
    void ReleaseMonitorData();

    void NotifyModeChange(
            HMONITOR                hMonitor,
            BOOL                    Windowed);
    
  };
  
}
