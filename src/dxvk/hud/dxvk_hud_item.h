#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "../../util/util_time.h"

#include "dxvk_hud_renderer.h"

namespace dxvk::hud {

  /**
   * \brief HUD item
   *
   * A single named item in the HUD that
   * can be enabled by the user.
   */
  class HudItem : public RcObject {

  public:

    virtual ~HudItem();

    /**
     * \brief Updates the HUD item
     * \param [in] time Current time
     */
    virtual void update(
            dxvk::high_resolution_clock::time_point time);

    /**
     * \brief Renders the HUD
     *
     * \param [in] renderer HUD renderer
     * \param [in] position Base offset
     * \returns Base offset for next item
     */
    virtual HudPos render(
            HudRenderer&      renderer,
            HudPos            position) = 0;

  };


  /**
   * \brief HUD item set
   *
   * Manages HUD items.
   */
  class HudItemSet {

  public:

    HudItemSet();

    ~HudItemSet();

    /**
     * \brief Updates the HUD
     * Updates all enabled HUD items.
     */
    void update();

    /**
     * \brief Renders the HUD
     *
     * \param [in] renderer HUD renderer
     * \returns Base offset for next item
     */
    void render(
            HudRenderer&      renderer);

    /**
     * \brief Creates a HUD item if enabled
     *
     * \tparam T The HUD item type
     * \param [in] name HUD item name
     * \param [in] args Constructor arguments
     */
    template<typename T, typename... Args>
    void add(const char* name, Args... args) {
      bool enable = m_enableFull;

      if (!enable) {
        auto entry = m_enabled.find(name);
        enable = entry != m_enabled.end();
      }

      if (enable)
        m_items.push_back(new T(std::forward<Args>(args)...));
    }

  private:

    bool                            m_enableFull = false;
    std::unordered_set<std::string> m_enabled;
    std::vector<Rc<HudItem>>        m_items;

  };

}