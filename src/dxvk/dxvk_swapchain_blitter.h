#pragma once

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_context.h"

namespace dxvk {

  /**
   * \brief Gamma control point
   */
  struct DxvkGammaCp {
    uint16_t r, g, b, a;
  };
  
  /**
   * \brief Swap chain blitter
   *
   * Provides common rendering code for blitting
   * rendered images to a swap chain image.
   */
  class DxvkSwapchainBlitter : public RcObject {
    
  public:

    DxvkSwapchainBlitter(const Rc<DxvkDevice>& device);
    ~DxvkSwapchainBlitter();

    /**
     * \brief Records presentation commands
     *
     * \param [in] ctx Context
     * \param [in] dstView Swap chain image view
     * \param [in] srcView Image to present
     * \param [in] dstRect Destination rectangle
     * \param [in] srcRect Back buffer rectangle
     */
    void presentImage(
            DxvkContext*        ctx,
      const Rc<DxvkImageView>&  dstView,
            VkRect2D            dstRect,
      const Rc<DxvkImageView>&  srcView,
            VkRect2D            srcRect);

    /**
     * \brief Sets gamma ramp
     *
     * If the number of control points is non-zero, this
     * will create a texture containing a gamma ramp that
     * will be used for presentation.
     * \param [in] cpCount Number of control points
     * \param [in] cpData Control point data
     */
    void setGammaRamp(
            uint32_t            cpCount,
      const DxvkGammaCp*        cpData);

  private:

    enum BindingIds : uint32_t {
      Image = 0,
      Gamma = 1,
    };

    struct PresenterArgs {
      VkOffset2D srcOffset;
      union {
        VkExtent2D srcExtent;
        VkOffset2D dstOffset;
      };
    };

    Rc<DxvkDevice>      m_device;

    Rc<DxvkShader>      m_fsCopy;
    Rc<DxvkShader>      m_fsBlit;
    Rc<DxvkShader>      m_fsResolve;
    Rc<DxvkShader>      m_vs;

    Rc<DxvkImage>       m_gammaImage;
    Rc<DxvkImageView>   m_gammaView;
    bool                m_gammaDirty = false;
    std::vector<DxvkGammaCp> m_gammaRamp;

    Rc<DxvkImage>       m_resolveImage;
    Rc<DxvkImageView>   m_resolveView;

    Rc<DxvkSampler>     m_samplerPresent;
    Rc<DxvkSampler>     m_samplerGamma;

    void draw(
            DxvkContext*        ctx,
      const Rc<DxvkShader>&     fs,
      const Rc<DxvkImageView>&  dstView,
            VkRect2D            dstRect,
      const Rc<DxvkImageView>&  srcView,
            VkRect2D            srcRect);

    void resolve(
            DxvkContext*        ctx,
      const Rc<DxvkImageView>&  dstView,
      const Rc<DxvkImageView>&  srcView);

    void updateGammaTexture(DxvkContext* ctx);

    void createSampler();

    void createShaders();

    void createResolveImage(
      const DxvkImageCreateInfo&  info);

    void destroyResolveImage();

  };
  
}