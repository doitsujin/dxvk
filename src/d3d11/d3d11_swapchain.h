#pragma once

#include "d3d11_texture.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../util/sync/sync_signal_win32.h"

namespace dxvk {
  
  class D3D11Device;
  class D3D11DXGIDevice;

  /**
   * \brief Gamma control point
   * 
   * Control points are stored as normalized
   * 16-bit unsigned integer values that will
   * be converted back to floats in the shader.
   */
  struct D3D11_VK_GAMMA_CP {
    uint16_t R, G, B, A;
  };

  class D3D11SwapChain : public ComObject<IDXGIVkSwapChain> {
    constexpr static uint32_t DefaultFrameLatency = 1;
  public:

    D3D11SwapChain(
            D3D11DXGIDevice*          pContainer,
            D3D11Device*              pDevice,
            HWND                      hWnd,
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
      const DXGI_SWAP_CHAIN_DESC1*    pDesc);

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

  private:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    Com<D3D11DXGIDevice, false> m_dxgiDevice;
    
    D3D11Device*            m_parent;
    HWND                    m_window;

    DXGI_SWAP_CHAIN_DESC1   m_desc;

    Rc<DxvkDevice>          m_device;
    Rc<DxvkContext>         m_context;

    Rc<vk::Presenter>       m_presenter;

    Rc<DxvkShader>          m_vertShader;
    Rc<DxvkShader>          m_fragShader;

    Rc<DxvkSampler>         m_samplerFitting;
    Rc<DxvkSampler>         m_samplerScaling;

    Rc<DxvkSampler>         m_gammaSampler;
    Rc<DxvkImage>           m_gammaTexture;
    Rc<DxvkImageView>       m_gammaTextureView;

    Rc<DxvkImage>           m_swapImage;
    Rc<DxvkImage>           m_swapImageResolve;
    Rc<DxvkImageView>       m_swapImageView;

    Rc<hud::Hud>            m_hud;

    DxvkInputAssemblyState  m_iaState;
    DxvkRasterizerState     m_rsState;
    DxvkMultisampleState    m_msState;
    DxvkDepthStencilState   m_dsState;
    DxvkLogicOpState        m_loState;
    DxvkBlendMode           m_blendMode;

    D3D11Texture2D*         m_backBuffer = nullptr;

    DxvkSubmitStatus        m_presentStatus;

    std::vector<Rc<DxvkImageView>> m_imageViews;

    uint64_t                m_frameId      = DXGI_MAX_SWAP_CHAIN_BUFFERS;
    uint32_t                m_frameLatency = DefaultFrameLatency;
    uint32_t                m_frameLatencyCap = 0;
    HANDLE                  m_frameLatencyEvent = nullptr;
    Rc<sync::Win32Fence>    m_frameLatencySignal;

    bool                    m_dirty = true;
    bool                    m_vsync = true;

    HRESULT PresentImage(UINT SyncInterval);

    void SubmitPresent(
            D3D11ImmediateContext*  pContext,
      const vk::PresenterSync&      Sync,
            uint32_t                FrameId);

    void SynchronizePresent();

    void RecreateSwapChain(
            BOOL                      Vsync);

    void CreateFrameLatencyEvent();

    void CreatePresenter();

    void CreateRenderTargetViews();

    void CreateBackBuffer();

    void CreateGammaTexture(
            UINT                NumControlPoints,
      const D3D11_VK_GAMMA_CP*  pControlPoints);
    
    void DestroyFrameLatencyEvent();

    void DestroyGammaTexture();
    
    void CreateHud();

    void InitRenderState();

    void InitSamplers();

    void InitShaders();

    void SignalFrameLatencyEvent();

    uint32_t GetActualFrameLatency();
    
    uint32_t PickFormats(
            DXGI_FORMAT               Format,
            VkSurfaceFormatKHR*       pDstFormats);
    
    uint32_t PickPresentModes(
            BOOL                      Vsync,
            VkPresentModeKHR*         pDstModes);
    
    uint32_t PickImageCount(
            UINT                      Preferred);
    
    VkFullScreenExclusiveEXT PickFullscreenMode();

    std::string GetApiName() const;

  };

}