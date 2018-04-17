#pragma once

#include <chrono>

#include "dxvk_hud_renderer.h"

namespace dxvk::hud {
  
  /**
   * \brief FPS display for the HUD
   * 
   * Displays the current frames per second.
   */
  class HudFps {
    using Clock     = std::chrono::high_resolution_clock;
    using TimeDiff  = std::chrono::microseconds;
    using TimePoint = typename Clock::time_point;
    
    constexpr static int64_t UpdateInterval = 500'000;
  public:
    
    HudFps();
    ~HudFps();
    
    void update();
    
    HudPos renderText(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
  private:
    
    std::string m_fpsString;
    
    TimePoint m_prevUpdate;
    int64_t  m_frameCount = 0;
    
  };
  
}