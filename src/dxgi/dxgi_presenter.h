#pragma once

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_surface.h"
#include "../dxvk/dxvk_swapchain.h"

#include "../dxvk/hud/dxvk_hud.h"

#include "../spirv/spirv_module.h"

#include "dxgi_include.h"

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
   * \brief Gamma input color
   * A floating-point color vector.
   */
  struct DXGI_VK_GAMMA_INPUT_COLOR {
    float R, G, B, A;
  };
  
  /**
   * \brief Gamma input control
   * 
   * Stores a scaling factor and a bias that shall
   * be applied to the input color before performing
   * the gamma lookup in the fragment shader.
   */
  struct DXGI_VK_GAMMA_INPUT_CONTROL {
    DXGI_VK_GAMMA_INPUT_COLOR Factor;
    DXGI_VK_GAMMA_INPUT_COLOR Offset;
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
  class DxgiPresenter : public RcObject {
    
  public:
    
    DxgiPresenter(
      const Rc<DxvkDevice>&         device,
            HWND                    window);
    
    ~DxgiPresenter();
      
    /**
     * \brief Initializes back buffer image
     * \param [in] image Back buffer image
     */
    void initBackBuffer(
      const Rc<DxvkImage>& image);
    
    /**
     * \brief Renders back buffer to the screen
     */
    void presentImage();
    
    /**
     * \brief Sets new back buffer
     * 
     * Recreates internal structures when
     * the back buffer image was replaced.
     * \param [in] image Back buffer image
     */
    void updateBackBuffer(
      const Rc<DxvkImage>& image);
    
    /**
     * \brief Recreats Vulkan swap chain
     * 
     * Only actually recreates the swap chain object
     * if any of the properties have changed. If no
     * properties have changed, this is a no-op.
     * \param [in] options New swap chain options
     */
    void recreateSwapchain(
      const DxvkSwapchainProperties& options);
    
    /**
     * \brief Picks a surface format based on a DXGI format
     * 
     * This will return a supported format that, if possible,
     * has properties similar to those of the DXGI format.
     * \param [in] fmt The DXGI format
     * \returns The Vulkan format
     */
    VkSurfaceFormatKHR pickSurfaceFormat(DXGI_FORMAT fmt) const;
    
    /**
     * \brief Picks a supported present mode
     * 
     * \param [in] preferred Preferred present mode
     * \returns An actually supported present mode
     */
    VkPresentModeKHR pickPresentMode(VkPresentModeKHR preferred) const;
    
    /**
     * \brief Sets gamma curve
     * 
     * Updates the gamma lookup texture.
     * \param [in] pGammaControl Input parameters
     * \param [in] pGammaCurve Gamma curve
     */
    void setGammaControl(
      const DXGI_VK_GAMMA_INPUT_CONTROL*  pGammaControl,
      const DXGI_VK_GAMMA_CURVE*          pGammaCurve);
    
  private:
    
    enum BindingIds : uint32_t {
      Sampler   = 0,
      Texture   = 1,
      GammaSmp  = 2,
      GammaTex  = 3,
      GammaUbo  = 4,
    };
    
    Rc<DxvkDevice>          m_device;
    Rc<DxvkContext>         m_context;
    
    Rc<DxvkSurface>         m_surface;
    Rc<DxvkSwapchain>       m_swapchain;
    
    Rc<DxvkSampler>         m_samplerFitting;
    Rc<DxvkSampler>         m_samplerScaling;
    
    Rc<DxvkImage>           m_backBuffer;
    Rc<DxvkImage>           m_backBufferResolve;
    Rc<DxvkImageView>       m_backBufferView;
    
    Rc<DxvkBuffer>          m_gammaUbo;
    Rc<DxvkSampler>         m_gammaSampler;
    Rc<DxvkImage>           m_gammaTexture;
    Rc<DxvkImageView>       m_gammaTextureView;
    
    Rc<hud::Hud>            m_hud;
    
    DxvkBlendMode           m_blendMode;
    DxvkSwapchainProperties m_options;
    
    Rc<DxvkSampler> createSampler(
            VkFilter              filter,
            VkSamplerAddressMode  addressMode);
    
    Rc<DxvkBuffer>    createGammaUbo();
    Rc<DxvkImage>     createGammaTexture();
    Rc<DxvkImageView> createGammaTextureView();
    
    Rc<DxvkShader> createVertexShader();
    Rc<DxvkShader> createFragmentShader();
    
  };
  
}
