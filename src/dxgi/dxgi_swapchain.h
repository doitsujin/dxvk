#pragma once

#include <memory>
#include <mutex>

#include "dxgi_interfaces.h"
#include "dxgi_monitor.h"
#include "dxgi_object.h"

#include "../d3d11/d3d11_interfaces.h"

#include "../spirv/spirv_module.h"

#include "../util/util_time.h"

#include "../wsi/wsi_window.h"
#include "../wsi/wsi_monitor.h"

namespace dxvk {
  
  class DxgiDevice;
  class DxgiFactory;
  class DxgiOutput;
  
  class DxgiSwapChain : public DxgiObject<IDXGISwapChain4> {
    
  public:
    
    DxgiSwapChain(
            DxgiFactory*                pFactory,
            IDXGIVkSwapChain*           pPresenter,
            HWND                        hWnd,
      const DXGI_SWAP_CHAIN_DESC1*      pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*  pFullscreenDesc,
            IUnknown*                   pDevice);
    
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
    
    dxvk::recursive_mutex           m_lockWindow;
    dxvk::mutex                     m_lockBuffer;

    Com<DxgiFactory>                m_factory;
    Com<IDXGIAdapter>               m_adapter;
    Com<IDXGIOutput1>               m_target;
    Com<IDXGIVkMonitorInfo>         m_monitorInfo;
    
    HWND                            m_window;
    DXGI_SWAP_CHAIN_DESC1           m_desc;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC m_descFs;
    UINT                            m_presentId;
    bool                            m_ModeChangeInProgress = false;

    Com<IDXGIVkSwapChain>           m_presenter;
    Com<IDXGIVkSwapChain1>          m_presenter1;
    Com<IDXGIVkSwapChain2>          m_presenter2;
    
    HMONITOR                        m_monitor;
    bool                            m_monitorHasOutput = true;
    bool                            m_frameStatisticsDisjoint = true;
    wsi::DxvkWindowState            m_windowState;

    double                          m_frameRateOption = 0.0;
    double                          m_frameRateRefresh = 0.0;
    double                          m_frameRateLimit = 0.0;
    uint32_t                        m_frameRateSyncInterval = 0u;
    bool                            m_is_d3d12;

    DXGI_COLOR_SPACE_TYPE           m_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    uint32_t                        m_globalHDRStateSerial = 0;
    bool                            m_hasLatencyControl = false;
    
    HRESULT EnterFullscreenMode(
            IDXGIOutput1            *pTarget);
    
    HRESULT LeaveFullscreenMode();
    
    HRESULT ChangeDisplayMode(
            IDXGIOutput1*           pOutput,
      const DXGI_MODE_DESC1*        pDisplayMode);
    
    HRESULT RestoreDisplayMode(
            HMONITOR                hMonitor);
    
    HRESULT GetSampleCount(
            UINT                    Count,
            VkSampleCountFlagBits*  pCount) const;
    
    HRESULT GetOutputFromMonitor(
            HMONITOR                Monitor,
            IDXGIOutput1**          ppOutput);
    
    HRESULT AcquireMonitorData(
            HMONITOR                hMonitor,
            DXGI_VK_MONITOR_DATA**  ppData);
    
    void ReleaseMonitorData();

    void UpdateGlobalHDRState();

    bool ValidateColorSpaceSupport(
            DXGI_FORMAT             Format,
            DXGI_COLOR_SPACE_TYPE   ColorSpace);

    HRESULT UpdateColorSpace(
            DXGI_FORMAT             Format,
            DXGI_COLOR_SPACE_TYPE   ColorSpace);

    void UpdateTargetFrameRate(
            UINT                    SyncInterval);

    HRESULT STDMETHODCALLTYPE PresentBase(
            UINT                      SyncInterval,
            UINT                      PresentFlags,
      const DXGI_PRESENT_PARAMETERS*  pPresentParameters);
  };
  
}
