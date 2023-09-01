#pragma once

#include "d3d11_texture.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../dxvk/dxvk_swapchain_blitter.h"

#include "../util/sync/sync_signal.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11DXGIDevice;

  class D3D11SwapChain : public ComObject<IDXGIVkSwapChain1> {
    constexpr static uint32_t DefaultFrameLatency = 1;
  public:

    D3D11SwapChain(
            D3D11DXGIDevice*          pContainer,
            D3D11Device*              pDevice,
            IDXGIVkSurfaceFactory*    pSurfaceFactory,
      const DXGI_SWAP_CHAIN_DESC1*    pDesc);
    
    ~D3D11SwapChain();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);

    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_SWAP_CHAIN_DESC1*    pDesc);

    HRESULT STDMETHODCALLTYPE GetAdapter(
            REFIID                    riid,
            void**                    ppvObject);
    
    HRESULT STDMETHODCALLTYPE GetDevice(
            REFIID                    riid,
            void**                    ppDevice);
    
    HRESULT STDMETHODCALLTYPE GetImage(
            UINT                      BufferId,
            REFIID                    riid,
            void**                    ppBuffer);

    UINT STDMETHODCALLTYPE GetImageIndex();

    UINT STDMETHODCALLTYPE GetFrameLatency();

    HANDLE STDMETHODCALLTYPE GetFrameLatencyEvent();

    HRESULT STDMETHODCALLTYPE ChangeProperties(
      const DXGI_SWAP_CHAIN_DESC1*    pDesc,
      const UINT*                     pNodeMasks,
            IUnknown* const*          ppPresentQueues);

    HRESULT STDMETHODCALLTYPE SetPresentRegion(
      const RECT*                     pRegion);

    HRESULT STDMETHODCALLTYPE SetGammaControl(
            UINT                      NumControlPoints,
      const DXGI_RGB*                 pControlPoints);

    HRESULT STDMETHODCALLTYPE SetFrameLatency(
            UINT                      MaxLatency);

    HRESULT STDMETHODCALLTYPE Present(
            UINT                      SyncInterval,
            UINT                      PresentFlags,
      const DXGI_PRESENT_PARAMETERS*  pPresentParameters);

    UINT STDMETHODCALLTYPE CheckColorSpaceSupport(
            DXGI_COLOR_SPACE_TYPE     ColorSpace);

    HRESULT STDMETHODCALLTYPE SetColorSpace(
            DXGI_COLOR_SPACE_TYPE     ColorSpace);

    HRESULT STDMETHODCALLTYPE SetHDRMetaData(
      const DXGI_VK_HDR_METADATA*     pMetaData);

    void STDMETHODCALLTYPE GetLastPresentCount(
            UINT64*                   pLastPresentCount);

    void STDMETHODCALLTYPE GetFrameStatistics(
            DXGI_VK_FRAME_STATISTICS* pFrameStatistics);

  private:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    Com<D3D11DXGIDevice, false> m_dxgiDevice;
    
    D3D11Device*              m_parent;
    Com<IDXGIVkSurfaceFactory> m_surfaceFactory;

    DXGI_SWAP_CHAIN_DESC1     m_desc;

    Rc<DxvkDevice>            m_device;
    Rc<DxvkContext>           m_context;

    Rc<Presenter>             m_presenter;

    Rc<DxvkImage>             m_swapImage;
    Rc<DxvkImageView>         m_swapImageView;
    Rc<DxvkSwapchainBlitter>  m_blitter;

    Rc<hud::Hud>              m_hud;

    Com<D3D11Texture2D, false> m_backBuffer;
    DxvkSubmitStatus          m_presentStatus;

    std::vector<Rc<DxvkImageView>> m_imageViews;

    uint64_t                m_frameId      = DXGI_MAX_SWAP_CHAIN_BUFFERS;
    uint32_t                m_frameLatency = DefaultFrameLatency;
    uint32_t                m_frameLatencyCap = 0;
    HANDLE                  m_frameLatencyEvent = nullptr;
    Rc<sync::CallbackFence> m_frameLatencySignal;

    bool                    m_dirty = true;

    VkColorSpaceKHR         m_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    std::optional<VkHdrMetadataEXT> m_hdrMetadata;
    bool m_dirtyHdrMetadata = true;

    dxvk::mutex               m_frameStatisticsLock;
    DXGI_VK_FRAME_STATISTICS  m_frameStatistics = { };

    HRESULT PresentImage(UINT SyncInterval);

    void SubmitPresent(
            D3D11ImmediateContext*  pContext,
      const PresenterSync&          Sync,
            uint32_t                Repeat);

    void SynchronizePresent();

    void RecreateSwapChain();

    void CreateFrameLatencyEvent();

    void CreatePresenter();

    VkResult CreateSurface(VkSurfaceKHR* pSurface);

    void CreateRenderTargetViews();

    void CreateBackBuffer();

    void CreateBlitter();

    void CreateHud();

    void DestroyFrameLatencyEvent();

    void SyncFrameLatency();

    uint32_t GetActualFrameLatency();
    
    uint32_t PickFormats(
            DXGI_FORMAT               Format,
            VkSurfaceFormatKHR*       pDstFormats);
    
    uint32_t PickImageCount(
            UINT                      Preferred);
    
    VkFullScreenExclusiveEXT PickFullscreenMode();

    std::string GetApiName() const;

  };

}