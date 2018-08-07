#pragma once

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_surface.h"
#include "../dxvk/dxvk_swapchain.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../spirv/spirv_module.h"

#include "dxgi_options.h"

namespace dxvk {
  
  constexpr uint32_t DXGI_VK_GAMMA_CP_COUNT = 1024;
  
  /**
   * \brief Gamma control point
   * 
   * Control points are stored as normalized
   * 16-bit unsigned integer values that will
   * be converted back to floats in the shader.
   */
  struct DXGI_VK_GAMMA_CP {
    uint16_t R, G, B, A;
  };
  
  /**
   * \brief Gamma curve
   * 
   * A collection of control points that
   * will be uploaded to the gamma texture.
   */
  struct DXGI_VK_GAMMA_CURVE {
    DXGI_VK_GAMMA_CP ControlPoints[DXGI_VK_GAMMA_CP_COUNT];
  };
  
  /**
   * \brief Maps color value to normalized integer
   * 
   * \param [in] x Input value, as floating point
   * \returns Corresponding normalized integer
   */
  inline uint16_t MapGammaControlPoint(float x) {
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return uint16_t(65535.0f * x);
  }
  
  /**
   * \brief Computes gamma control point location
   * 
   * \param [in] CpIndex Control point ID
   * \returns Location of the control point
   */
  inline float GammaControlPointLocation(uint32_t CpIndex) {
    return float(CpIndex) / float(DXGI_VK_GAMMA_CP_COUNT - 1);
  }
  
  /**
   * \brief DXGI presenter
   * 
   * Renders the back buffer from the
   * \ref DxgiSwapChain to the Vulkan
   * swap chain.
   */
  class DxgiVkPresenter : public RcObject {
    
  public:
    
    DxgiVkPresenter(
      const DxgiOptions*            pOptions,
      const Rc<DxvkDevice>&         device,
            HWND                    window);
    
    ~DxgiVkPresenter();
      
    /**
     * \brief Initializes back buffer image
     * \param [in] image Back buffer image
     */
    void InitBackBuffer(const Rc<DxvkImage>& Image);
    
    /**
     * \brief Renders back buffer to the screen
     * \param [in] SyncInterval Vsync interval
     */
    void PresentImage(UINT SyncInterval, const Rc<DxvkEvent>& SyncEvent);
    
    /**
     * \brief Sets new back buffer
     * 
     * Recreates internal structures when
     * the back buffer image was replaced.
     * \param [in] image Back buffer image
     */
    void UpdateBackBuffer(const Rc<DxvkImage>& Image);
    
    /**
     * \brief Recreats Vulkan swap chain
     * 
     * Only actually recreates the swap chain object
     * if any of the properties have changed. If no
     * properties have changed, this is a no-op.
     * \param [in] Format New surface format
     * \param [in] PresentMode Present mode
     * \param [in] WindowSize Window size
     */
    void RecreateSwapchain(
            DXGI_FORMAT       Format,
            VkPresentModeKHR  PresentMode,
            VkExtent2D        WindowSize);
    
    /**
     * \brief Sets gamma curve
     * 
     * Updates the gamma lookup texture.
     * \param [in] pGammaControl Input parameters
     * \param [in] pGammaCurve Gamma curve
     */
    void SetGammaControl(
      const DXGI_VK_GAMMA_CURVE*          pGammaCurve);
    
  private:
    
    enum BindingIds : uint32_t {
      Sampler   = 0,
      Texture   = 1,
      GammaSmp  = 2,
      GammaTex  = 3,
    };
    
    HWND                    m_window;
    
    Rc<DxvkDevice>          m_device;
    Rc<DxvkContext>         m_context;
    
    Rc<DxvkSurface>         m_surface;
    Rc<DxvkSwapchain>       m_swapchain;

    Rc<DxvkShader>          m_vertShader;
    Rc<DxvkShader>          m_fragShader;
    
    Rc<DxvkSampler>         m_samplerFitting;
    Rc<DxvkSampler>         m_samplerScaling;
    
    Rc<DxvkImage>           m_backBuffer;
    Rc<DxvkImage>           m_backBufferResolve;
    Rc<DxvkImageView>       m_backBufferView;
    
    Rc<DxvkSampler>         m_gammaSampler;
    Rc<DxvkImage>           m_gammaTexture;
    Rc<DxvkImageView>       m_gammaTextureView;
    
    Rc<hud::Hud>            m_hud;

    DxvkInputAssemblyState  m_iaState;
    DxvkRasterizerState     m_rsState;
    DxvkMultisampleState    m_msState;
    DxvkDepthStencilState   m_dsState;
    DxvkLogicOpState        m_loState;
    DxvkBlendMode           m_blendMode;
    DxvkSwapchainProperties m_options;
    
    VkSurfaceFormatKHR PickSurfaceFormat(DXGI_FORMAT Fmt) const;
    
    VkPresentModeKHR PickPresentMode(VkPresentModeKHR Preferred) const;
    
    Rc<DxvkSurface> CreateSurface();
    
    Rc<DxvkSampler> CreateSampler(
            VkFilter              Filter,
            VkSamplerAddressMode  AddressMode);
    
    Rc<DxvkImage>     CreateGammaTexture();
    Rc<DxvkImageView> CreateGammaTextureView();
    
    Rc<DxvkShader> CreateVertexShader();
    Rc<DxvkShader> CreateFragmentShader();
    
  };
  
}
