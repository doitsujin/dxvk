#pragma once

#include "../util/util_time.h"

#include "dxvk_hud_config.h"
#include "dxvk_hud_renderer.h"

namespace dxvk::hud {
  
  /**
   * \brief FPS display for the HUD
   * 
   * Displays the current frames per second.
   */
  class HudFps {
    using Clock     = dxvk::high_resolution_clock;
    using TimeDiff  = std::chrono::microseconds;
    using TimePoint = typename Clock::time_point;
    
    constexpr static uint32_t NumDataPoints  = 300;
    constexpr static int64_t  UpdateInterval = 500'000;
  public:
    
    HudFps(HudElements elements);
    ~HudFps();
    
    void update();
    
    HudPos render(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
  private:
    
    const HudElements m_elements;
    
    std::string m_fpsString;
    
    TimePoint m_prevFpsUpdate;
    TimePoint m_prevFtgUpdate;
    int64_t   m_frameCount = 0;
    
    std::array<float, NumDataPoints>  m_dataPoints  = {};
    uint32_t                          m_dataPointId = 0;
    
    HudPos renderFpsText(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos renderFrametimeGraph(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
  };
  
}