#pragma once

#include "d3d9_device_child.h"
#include "d3d9_device.h"
#include "d3d9_format.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../util/sync/sync_signal.h"

#include <vector>

namespace dxvk {

  class D3D9Surface;

  /**
   * \brief Gamma control point
   * 
   * Control points are stored as normalized
   * 16-bit unsigned integer values that will
   * be converted back to floats in the shader.
   */
  struct D3D9_VK_GAMMA_CP {
    uint16_t R, G, B, A;
  };

  using D3D9SwapChainExBase = D3D9DeviceChild<IDirect3DSwapChain9Ex>;
  class D3D9SwapChainEx final : public D3D9SwapChainExBase {
    static constexpr uint32_t NumControlPoints = 256;
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

    void    Reset(
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

  private:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    
    struct WindowState {
      LONG style   = 0;
      LONG exstyle = 0;
      RECT rect    = { 0, 0, 0, 0 };
    };

    D3DPRESENT_PARAMETERS   m_presentParams;
    D3DGAMMARAMP            m_ramp;

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

    Rc<DxvkImage>           m_resolveImage;
    Rc<DxvkImageView>       m_resolveImageView;

    Rc<hud::Hud>            m_hud;

    DxvkInputAssemblyState  m_iaState;
    DxvkRasterizerState     m_rsState;
    DxvkMultisampleState    m_msState;
    DxvkDepthStencilState   m_dsState;
    DxvkLogicOpState        m_loState;
    DxvkBlendMode           m_blendMode;

    std::vector<Com<D3D9Surface, false>> m_backBuffers;
    
    RECT                    m_srcRect;
    RECT                    m_dstRect;

    DxvkSubmitStatus        m_presentStatus;

    std::vector<Rc<DxvkImageView>> m_imageViews;


    uint64_t                m_frameId           = D3D9DeviceEx::MaxFrameLatency;
    uint32_t                m_frameLatencyCap   = 0;
    Rc<sync::Fence>         m_frameLatencySignal;

    bool                    m_dirty    = true;
    bool                    m_vsync    = true;

    bool                    m_dialog;
    bool                    m_lastDialog = false;

    HWND                    m_window   = nullptr;
    HMONITOR                m_monitor  = nullptr;

    WindowState             m_windowState;

    void PresentImage(UINT PresentInterval);

    void SubmitPresent(const vk::PresenterSync& Sync, uint32_t FrameId);

    void SynchronizePresent();

    void RecreateSwapChain(
        BOOL                      Vsync);

    void CreatePresenter();

    void CreateRenderTargetViews();

    void DestroyBackBuffers();

    void CreateBackBuffers(
            uint32_t            NumBackBuffers);

    void CreateGammaTexture(
            UINT                NumControlPoints,
      const D3D9_VK_GAMMA_CP*   pControlPoints);

    void DestroyGammaTexture();

    void CreateHud();

    void InitRenderState();

    void InitSamplers();

    void InitShaders();

    void InitRamp();

    uint32_t GetActualFrameLatency();

    uint32_t PickFormats(
            D3D9Format                Format,
            VkSurfaceFormatKHR*       pDstFormats);
    
    uint32_t PickPresentModes(
            BOOL                      Vsync,
            VkPresentModeKHR*         pDstModes);
    
    uint32_t PickImageCount(
            UINT                      Preferred);

    void NormalizePresentParameters(D3DPRESENT_PARAMETERS* pPresentParams);

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

  };

}