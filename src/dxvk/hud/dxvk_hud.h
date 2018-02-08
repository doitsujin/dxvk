#pragma once

#include "../dxvk_device.h"

#include "../util/util_env.h"

#include "dxvk_hud_devinfo.h"
#include "dxvk_hud_fps.h"
#include "dxvk_hud_text.h"

namespace dxvk::hud {
  
  struct HudUniformData {
    VkExtent2D surfaceSize;
  };
  
  /**
   * \brief DXVK HUD
   * 
   * Can be used by the presentation backend to
   * display performance and driver information.
   */
  class Hud : public RcObject {
    
  public:
    
    explicit Hud(
      const Rc<DxvkDevice>& device);
    
    ~Hud();
    
    /**
     * \brief Renders the HUD
     * 
     * Recreates the render targets for the HUD
     * in case the surface size has changed.
     * \param [in] size Render target size
     */
    void render(VkExtent2D size);
    
    /**
     * \brief Rendered image
     * 
     * Returns the rendered image from
     * the previous call to \ref render.
     * \returns The image view
     */
    Rc<DxvkImageView> texture() const {
      return m_renderTargetView;
    }
    
    /**
     * \brief Creates the HUD
     * 
     * Creates and initializes the HUD if the
     * \c DXVK_HUD environment variable is set.
     * \param [in] device The DXVK device
     * \returns HUD object, if it was created.
     */
    static Rc<Hud> createHud(
      const Rc<DxvkDevice>& device);
    
  private:
    
    const Rc<DxvkDevice>  m_device;
    const Rc<DxvkContext> m_context;
    
    HudTextRenderer       m_textRenderer;
    VkExtent2D            m_surfaceSize = { 0, 0 };
    
    Rc<DxvkBuffer>        m_uniformBuffer;
    Rc<DxvkImage>         m_renderTarget;
    Rc<DxvkImageView>     m_renderTargetView;
    Rc<DxvkFramebuffer>   m_renderTargetFbo;
    
    HudDeviceInfo         m_hudDeviceInfo;
    HudFps                m_hudFps;
    
    void renderText();
    
    Rc<DxvkBuffer> createUniformBuffer();
    
    void updateUniformBuffer();
    void beginRenderPass(bool initFbo);
    void endRenderPass();
    
    void setupFramebuffer(VkExtent2D size);
    void setupConstantState();
    
  };
  
}