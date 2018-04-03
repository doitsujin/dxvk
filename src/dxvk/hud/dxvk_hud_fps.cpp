#include "dxvk_hud_fps.h"

#include <iomanip>

namespace dxvk::hud {
  
  HudFps::HudFps()
  : m_fpsString("FPS: "),
    m_prevUpdate(Clock::now()) {
    
  }
  
  
  HudFps::~HudFps() {
    
  }
  

  void HudFps::update() {
    m_frameCount += 1;
    
    const TimePoint now = Clock::now();
    const TimeDiff elapsed = std::chrono::duration_cast<TimeDiff>(now - m_prevUpdate);
    
    if (elapsed.count() >= UpdateInterval) {
      const int64_t fps = (10'000'000ll * m_frameCount) / elapsed.count();
      m_fpsString = str::format("FPS: ", fps / 10, ".", fps % 10);
      
      m_prevUpdate = now;
      m_frameCount = 0;
    }
  }
  
  
  HudPos HudFps::renderText(
    const Rc<DxvkContext>&  context,
          HudTextRenderer&  renderer,
          HudPos            position) {
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_fpsString);
    
    return HudPos { position.x, position.y + 24 };
  }
  
}