#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"
#include "d3d9_device.h"
#include "d3d9_common_texture.h"

#include "../dxvk/hud/dxvk_hud.h"

namespace dxvk {

  struct D3D9PresenterDesc {
    D3D9Format format;
    uint32_t width;
    uint32_t height;
    uint32_t bufferCount;
    uint32_t presentInterval;
    D3DMULTISAMPLE_TYPE multisample;
  };

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

  class D3D9Presenter : public RcObject {

  public:

    constexpr static uint32_t GammaPointCount = 256;

    D3D9Presenter(
            Direct3DDevice9Ex*  parent,
            HWND                window,
      const D3D9PresenterDesc*  desc,
            DWORD               gammaFlags,
      const D3DGAMMARAMP*       gammaRamp);

    void recreateSwapChain(const D3D9PresenterDesc* desc);

    inline HWND window() {
      return m_window;
    }

    void present();

    Rc<Direct3DCommonTexture9> getBackBuffer() {
      return m_backBuffer;
    }

    void setGammaRamp(
      DWORD               Flags,
      const D3DGAMMARAMP* pRamp);

  private:

    void createBackBuffer();

    void createHud();

    void initRenderState();

    void initSamplers();

    void initShaders();

    void createGammaTexture(const D3D9_VK_GAMMA_CP* pControlPoints);

    uint32_t pickFormats(
      D3D9Format          format,
      VkSurfaceFormatKHR* dstFormats);

    uint32_t pickPresentModes(
      bool                vsync,
      VkPresentModeKHR*   dstModes);

    uint32_t pickImageCount(
      uint32_t            preferred);

    void createPresenter();

    VkFormat makeSrgb(VkFormat format);

    void createRenderTargetViews();

    enum BindingIds : uint32_t {
      Sampler = 0,
      Texture = 1,
      GammaSmp = 2,
      GammaTex = 3,
    };

    Direct3DDevice9Ex* m_parent;
    Rc<DxvkDevice>     m_device;
    Rc<DxvkContext> m_context;
    Rc<vk::Presenter> m_presenter;

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
    DxvkBlendMode m_blendMode;

    HWND m_window;

    std::vector<Rc<DxvkImageView>> m_imageViews;
    std::vector<Rc<DxvkImageView>> m_imageViewsSrgb;

    Rc<Direct3DCommonTexture9> m_backBuffer;

    D3D9PresenterDesc m_desc;

  };

}