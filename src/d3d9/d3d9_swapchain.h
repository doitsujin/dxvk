#pragma once

#include "d3d9_device_child.h"
#include "d3d9_device.h"
#include "d3d9_format.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../util/sync/sync_signal.h"

#include <vector>
#include <list>

namespace dxvk {

  class D3D9Surface;
  class D3D9SwapChainEx;

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

  class D3D9PresentationInfo {

  public:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    D3D9PresentationInfo(D3D9DeviceEx* pDevice);

    Rc<DxvkDevice>  device;
    Rc<DxvkContext> context;
    Rc<hud::Hud>    hud;

    std::vector<Com<D3D9Surface, false>> backBuffers;

    Rc<DxvkShader>          vertShader;
    Rc<DxvkShader>          fragShader;

    Rc<DxvkSampler>         samplerFitting;
    Rc<DxvkSampler>         samplerScaling;

    Rc<DxvkSampler>         gammaSampler;
    Rc<DxvkImage>           gammaTexture;
    Rc<DxvkImageView>       gammaTextureView;

    Rc<DxvkImage>           resolveImage;
    Rc<DxvkImageView>       resolveImageView;

    DxvkInputAssemblyState  iaState = {};
    DxvkRasterizerState     rsState = {};
    DxvkMultisampleState    msState = {};
    DxvkDepthStencilState   dsState = {};
    DxvkLogicOpState        loState = {};
    DxvkBlendMode           blendMode = {};

    uint64_t                frameId           = D3D9DeviceEx::MaxFrameLatency;
    uint32_t                frameLatencyCap   = 0;
    Rc<sync::Fence>         frameLatencySignal;

    bool                    dialog;
  };

  class D3D9Presenter : public RcObject {

  public:

    D3D9Presenter(
      const D3D9PresentationInfo& Info,
            D3D9SwapChainEx*      pParent,
            HWND                  hWindow);

    ~D3D9Presenter();

    HRESULT Present(
            UINT  SyncInterval,
      const RECT* pSourceRect,
      const RECT* pDestRect);

    void SubmitPresent(const vk::PresenterSync& Sync, uint32_t FrameId);

    void SynchronizePresent();

    void RecreateSwapChain(BOOL Vsync);

    void CreatePresenter();

    void CreateRenderTargetViews();

    void CreateCopyImage();

    VkExtent2D GetPresentExtent();

    HWND GetWindow() const { return m_window; }

    void MarkDirty() { m_dirty = true; }

  private:

    void PresentImage(
            UINT  SyncInterval,
      const RECT* pSourceRect,
      const RECT* pDestRect);

    D3D9SwapChainEx*  m_parent;

    HWND              m_window;

    Rc<vk::Presenter> m_presenter;

    VkExtent2D        m_presentExtent = {};

    DxvkSubmitStatus  m_presentStatus;

    bool              m_dirty = true;
    bool              m_vsync = true;

    bool              m_partialPresented = false;
    bool              m_lastDialog = false;

    Rc<DxvkImage>     m_copyImage;
    Rc<DxvkImageView> m_copyImageView;

    std::vector<Rc<DxvkImageView>> m_imageViews;

    const D3D9PresentationInfo& m_info;

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

  private:

    friend class D3D9Presenter;
    
    struct WindowState {
      LONG style   = 0;
      LONG exstyle = 0;
      RECT rect    = { 0, 0, 0, 0 };
    };

    D3D9PresentationInfo    m_info;

    D3DPRESENT_PARAMETERS   m_presentParams;
    D3DGAMMARAMP            m_ramp;

    Rc<D3D9Presenter>            m_mainPresenter;
    std::list<Rc<D3D9Presenter>> m_presenters;

    HMONITOR                m_monitor  = nullptr;

    MONITORINFOEXW          m_monInfo;

    WindowState             m_windowState;

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

    void    UpdateMonitorInfo();

    VkFullScreenExclusiveEXT PickFullscreenMode();

    std::string GetApiName();

  };

}