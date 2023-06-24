#pragma once

#include "d3d9_device_child.h"
#include "d3d9_device.h"
#include "d3d9_format.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../dxvk/dxvk_swapchain_blitter.h"

#include "../util/sync/sync_signal.h"

#include "../wsi/wsi_window.h"
#include "../wsi/wsi_monitor.h"

#include <vector>

namespace dxvk {

  class D3D9Surface;
  class D3D9SwapChainEx;

  class D3D9VkExtSwapchain final : public ID3D9VkExtSwapchain {
  public:
    D3D9VkExtSwapchain(D3D9SwapChainEx *pSwapChain);
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    BOOL STDMETHODCALLTYPE CheckColorSpaceSupport(
            VkColorSpaceKHR           ColorSpace);

    HRESULT STDMETHODCALLTYPE SetColorSpace(
            VkColorSpaceKHR           ColorSpace);

    HRESULT STDMETHODCALLTYPE SetHDRMetaData(
      const VkHdrMetadataEXT          *pHDRMetadata);

    HRESULT STDMETHODCALLTYPE GetCurrentOutputDesc(
            D3D9VkExtOutputMetadata   *pOutputDesc);

    void STDMETHODCALLTYPE UnlockAdditionalFormats();

  private:
    D3D9SwapChainEx *m_swapchain;
  };

  struct D3D9WindowContext {
    Rc<Presenter>                  presenter;
    std::vector<Rc<DxvkImageView>> imageViews;

    uint64_t                       frameId = D3D9DeviceEx::MaxFrameLatency;
    Rc<sync::Fence>                frameLatencySignal;
  };

  using D3D9SwapChainExBase = D3D9DeviceChild<IDirect3DSwapChain9Ex>;
  class D3D9SwapChainEx final : public D3D9SwapChainExBase {
    static constexpr uint32_t NumControlPoints = 256;

    friend class D3D9VkExtSwapchain;
  public:

    D3D9SwapChainEx(
            D3D9DeviceEx*          pDevice,
            D3DPRESENT_PARAMETERS* pPresentParams,
      const D3DDISPLAYMODEEX*      pFullscreenDisplayMode);

    ~D3D9SwapChainEx();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Present(
      const RECT*    pSourceRect,
      const RECT*    pDestRect,
            HWND     hDestWindowOverride,
      const RGNDATA* pDirtyRegion,
            DWORD    dwFlags);

#ifdef _WIN32
    HRESULT PresentImageGDI(HWND Window);
#endif

    HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSurface9* pDestSurface);

    HRESULT STDMETHODCALLTYPE GetBackBuffer(
            UINT                iBackBuffer,
            D3DBACKBUFFER_TYPE  Type,
            IDirect3DSurface9** ppBackBuffer);

    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* pRasterStatus);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters);

    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount);

    HRESULT STDMETHODCALLTYPE GetPresentStats(D3DPRESENTSTATS* pPresentationStatistics);

    HRESULT STDMETHODCALLTYPE GetDisplayModeEx(D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation);

    HRESULT Reset(
            D3DPRESENT_PARAMETERS* pPresentParams,
            D3DDISPLAYMODEEX*      pFullscreenDisplayMode);

    HRESULT WaitForVBlank();

    void    SetGammaRamp(
            DWORD         Flags,
      const D3DGAMMARAMP* pRamp);

    void    GetGammaRamp(D3DGAMMARAMP* pRamp);

    void    Invalidate(HWND hWindow);

    HRESULT SetDialogBoxMode(bool bEnableDialogs);

    D3D9Surface* GetBackBuffer(UINT iBackBuffer);

    const D3DPRESENT_PARAMETERS* GetPresentParams() const { return &m_presentParams; }

    void SyncFrameLatency();

    bool HasFormatsUnlocked() const { return m_unlockAdditionalFormats; }

    void DestroyBackBuffers();

    void SetApiName(const char* name);

    void UpdateWindowCtx();

  private:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    D3DPRESENT_PARAMETERS     m_presentParams;
    D3DGAMMARAMP              m_ramp;

    Rc<DxvkDevice>            m_device;
    Rc<DxvkContext>           m_context;
    Rc<DxvkSwapchainBlitter>  m_blitter;

    std::unordered_map<
      HWND,
      D3D9WindowContext>      m_presenters;

    D3D9WindowContext*        m_wctx = nullptr;

    Rc<hud::Hud>              m_hud;

    std::vector<Com<D3D9Surface, false>> m_backBuffers;
    
    RECT                      m_srcRect;
    RECT                      m_dstRect;
    VkExtent2D                m_swapchainExtent = { 0u, 0u };
    bool                      m_partialCopy = false;

    DxvkSubmitStatus          m_presentStatus;

    uint32_t                  m_frameLatencyCap = 0;

    bool                      m_dirty    = true;
    bool                      m_dialog   = false;
    bool                      m_lastDialog = false;

    HWND                      m_window   = nullptr;
    HMONITOR                  m_monitor  = nullptr;

    wsi::DxvkWindowState      m_windowState;

    double                    m_displayRefreshRate = 0.0;

    const char*               m_apiName  = nullptr;

    bool                      m_warnedAboutGDIFallback = false;

    VkColorSpaceKHR           m_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    std::optional<VkHdrMetadataEXT> m_hdrMetadata;
    bool m_dirtyHdrMetadata = true;
    bool m_unlockAdditionalFormats = false;

    D3D9VkExtSwapchain m_swapchainExt;

    void PresentImage(UINT PresentInterval);

    void SubmitPresent(const PresenterSync& Sync, uint32_t Repeat);

    void SynchronizePresent();

    void RecreateSwapChain();

    void CreatePresenter();

    VkResult CreateSurface(VkSurfaceKHR* pSurface);

    void CreateRenderTargetViews();

    HRESULT CreateBackBuffers(
            uint32_t            NumBackBuffers);

    void CreateBlitter();

    void CreateHud();

    void InitRamp();

    uint32_t GetActualFrameLatency();

    uint32_t PickFormats(
            D3D9Format                Format,
            VkSurfaceFormatKHR*       pDstFormats);
    
    uint32_t PickImageCount(
            UINT                      Preferred);

    void NormalizePresentParameters(D3DPRESENT_PARAMETERS* pPresentParams);

    void NotifyDisplayRefreshRate(
            double                  RefreshRate);

    HRESULT EnterFullscreenMode(
            D3DPRESENT_PARAMETERS*  pPresentParams,
      const D3DDISPLAYMODEEX*       pFullscreenDisplayMode);
    
    HRESULT LeaveFullscreenMode();
    
    HRESULT ChangeDisplayMode(
            D3DPRESENT_PARAMETERS*  pPresentParams,
      const D3DDISPLAYMODEEX*       pFullscreenDisplayMode);
    
    HRESULT RestoreDisplayMode(HMONITOR hMonitor);

    bool    UpdatePresentRegion(const RECT* pSourceRect, const RECT* pDestRect);

    VkExtent2D GetPresentExtent();

    VkFullScreenExclusiveEXT PickFullscreenMode();

    std::string GetApiName();

    const Com<D3D9Surface, false>& GetFrontBuffer() const {
      return m_backBuffers.back();
    }

    bool HasFrontBuffer() const {
      if (m_presentParams.SwapEffect == D3DSWAPEFFECT_COPY)
        return false;

      if (m_presentParams.SwapEffect == D3DSWAPEFFECT_COPY_VSYNC)
        return false;

      // Tests show that SWAPEEFFECT_DISCARD + 1 backbuffer in windowed mode behaves identically to SWAPEFFECT_COPY
      // For SWAPEFFECT_COPY we don't swap buffers but do another blit to the front buffer instead.
      if (m_presentParams.SwapEffect == D3DSWAPEFFECT_DISCARD && m_presentParams.BackBufferCount == 1 && m_presentParams.Windowed)
        return false;

      return true;
    }
  };

}
