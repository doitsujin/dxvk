#pragma once

#include "../dxvk_device.h"

#include "dxvk_hud_item.h"
#include "dxvk_hud_renderer.h"

namespace dxvk::hud {

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
     * \param [in] ctx Context objects for rendering
     * \param [in] dstView Swap chain image view
     */
    void render(
      const DxvkContextObjects& ctx,
      const Rc<DxvkImageView>&  dstView);

    /**
     * \brief Checks whether the HUD is empty
     * \returns \c true if the HUD is empty
     */
    bool empty() const {
      return m_hudItems.empty();
    }

    /**
     * \brief Adds a HUD item if enabled
     *
     * \tparam T The HUD item type
     * \param [in] name HUD item name
     * \param [in] args Constructor arguments
     */
    template<typename T, typename... Args>
    Rc<T> addItem(const char* name, int32_t at, Args... args) {
      return m_hudItems.add<T>(name, at, std::forward<Args>(args)...);
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
    
    Rc<DxvkDevice>        m_device;
    
    HudRenderer           m_renderer;
    HudItemSet            m_hudItems;

    HudOptions            m_options;
    
  };
  
}