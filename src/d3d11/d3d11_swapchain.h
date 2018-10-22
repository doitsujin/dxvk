#pragma once

#include "d3d11_texture.h"

#include "../dxvk/dxvk_surface.h"
#include "../dxvk/dxvk_swapchain.h"

#include "../dxvk/hud/dxvk_hud.h"

namespace dxvk {
  
  class D3D11Device;

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

  public:

    D3D11SwapChain(
            D3D11Device*            pDevice,
            HWND                    hWnd,
      const DXGI_SWAP_CHAIN_DESC1*  pDesc);
    
    ~D3D11SwapChain();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_SWAP_CHAIN_DESC1*    pDesc);

    HRESULT STDMETHODCALLTYPE GetImage(
            UINT                      BufferId,
            REFIID                    riid,
            void**                    ppBuffer);

    UINT STDMETHODCALLTYPE GetImageIndex();

    HRESULT STDMETHODCALLTYPE ChangeProperties(
      const DXGI_SWAP_CHAIN_DESC1*    pDesc);

    HRESULT STDMETHODCALLTYPE SetPresentRegion(
      const RECT*                     pRegion);

    HRESULT STDMETHODCALLTYPE SetGammaControl(
            UINT                      NumControlPoints,
      const DXGI_RGB*                 pControlPoints);

    HRESULT STDMETHODCALLTYPE Present(
            UINT                      SyncInterval,
            UINT                      PresentFlags,
      const DXGI_PRESENT_PARAMETERS*  pPresentParameters);
    
  private:

    enum BindingIds : uint32_t {
      Sampler   = 0,
      Texture   = 1,
      GammaSmp  = 2,
      GammaTex  = 3,
    };

    Com<IDXGIVkDevice>      m_dxgiDevice;
    
    D3D11Device*            m_parent;
    HWND                    m_window;

    DXGI_SWAP_CHAIN_DESC1   m_desc;

    Rc<DxvkDevice>          m_device;
    Rc<DxvkContext>         m_context;

    Rc<DxvkSurface>         m_surface;
    Rc<DxvkSwapchain>       m_swapchain;

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

    bool                    m_dirty = true;
    bool                    m_vsync = true;

    void PresentImage(UINT SyncInterval);

    void FlushImmediateContext();

    void CreateBackBuffer();

    void CreateGammaTexture(
            UINT                NumControlPoints,
      const D3D11_VK_GAMMA_CP*  pControlPoints);
    
    void CreateSurface();

    void CreateSwapChain();

    void CreateHud();

    void InitRenderState();

    void InitSamplers();

    void InitShaders();
    
    VkSurfaceFormatKHR PickSurfaceFormat() const;
    
    VkPresentModeKHR PickPresentMode() const;

  };

}