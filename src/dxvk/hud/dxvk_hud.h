#pragma once

#include "../dxvk_device.h"

#include "dxvk_hud_item.h"
#include "dxvk_hud_renderer.h"

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
    
    Hud(const Rc<DxvkDevice>& device);
    
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
      const Rc<DxvkContext>&  ctx,
            VkSurfaceFormatKHR surfaceFormat,
            VkExtent2D        surfaceSize);

    /**
     * \brief Adds a HUD item if enabled
     *
     * \tparam T The HUD item type
     * \param [in] name HUD item name
     * \param [in] args Constructor arguments
     */
    template<typename T, typename... Args>
    void addItem(const char* name, int32_t at, Args... args) {
      m_hudItems.add<T>(name, at, std::forward<Args>(args)...);
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
    
    DxvkRasterizerState   m_rsState;
    DxvkBlendMode         m_blendMode;

    HudUniformData        m_uniformData;
    HudRenderer           m_renderer;
    HudItemSet            m_hudItems;

    float                 m_scale;

    void setupRendererState(
      const Rc<DxvkContext>&  ctx,
            VkSurfaceFormatKHR surfaceFormat,
            VkExtent2D        surfaceSize);

    void resetRendererState(
      const Rc<DxvkContext>&  ctx);

    void renderHudElements(
      const Rc<DxvkContext>&  ctx);
    
  };
  
}