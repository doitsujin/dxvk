#pragma once

#include "../dxvk_device.h"

#include "../util/util_env.h"

#include "dxvk_hud_config.h"
#include "dxvk_hud_devinfo.h"
#include "dxvk_hud_fps.h"
#include "dxvk_hud_renderer.h"
#include "dxvk_hud_stats.h"

namespace dxvk::hud {
  
  /**
   * \brief HUD uniform data
   * Shader data for the HUD.
   */
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
    
    Hud(
      const Rc<DxvkDevice>& device,
      const HudConfig&      config);
    
    ~Hud();
    
    /**
     * \brief Update HUD
     * 
     * Updates the data to display.
     * Should be called once per frame.
     */
    void update();

    /**
     * \brief Render HUD
     * 
     * Renders the HUD to the given context.
     * \param [in] ctx Device context
     * \param [in] surfaceSize Image size, in pixels
     */
    void render(
      const Rc<DxvkContext>& ctx,
            VkExtent2D       surfaceSize);
    
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
    
    const HudConfig       m_config;
    const Rc<DxvkDevice>  m_device;
    
    Rc<DxvkBuffer>        m_uniformBuffer;

    DxvkRasterizerState   m_rsState;
    DxvkBlendMode         m_blendMode;

    HudUniformData        m_uniformData;
    HudRenderer           m_renderer;
    HudDeviceInfo         m_hudDeviceInfo;
    HudFps                m_hudFramerate;
    HudStats              m_hudStats;

    void setupRendererState(
      const Rc<DxvkContext>&  ctx);

    void renderHudElements(
      const Rc<DxvkContext>&  ctx);

    void updateUniformBuffer(
      const Rc<DxvkContext>&  ctx,
      const HudUniformData&   data);
    
    Rc<DxvkBuffer> createUniformBuffer();
    
  };
  
}