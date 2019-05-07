#include "dxvk_hud_fps.h"

#include <cmath>
#include <iomanip>

namespace dxvk::hud {
  
  HudFps::HudFps(HudElements elements)
  : m_elements  (elements),
    m_fpsString ("FPS: "),
    m_prevFpsUpdate(Clock::now()),
    m_prevFtgUpdate(Clock::now()) {
    
  }
  
  
  HudFps::~HudFps() {
    
  }
  

  void HudFps::update() {
    m_frameCount += 1;
    
    TimePoint now = Clock::now();
    TimeDiff elapsedFps = std::chrono::duration_cast<TimeDiff>(now - m_prevFpsUpdate);
    TimeDiff elapsedFtg = std::chrono::duration_cast<TimeDiff>(now - m_prevFtgUpdate);
    m_prevFtgUpdate = now;
    
    // Update FPS string
    if (elapsedFps.count() >= UpdateInterval) {
      const int64_t fps = (10'000'000ll * m_frameCount) / elapsedFps.count();
      m_fpsString = str::format("FPS: ", fps / 10, ".", fps % 10);
      
      m_prevFpsUpdate = now;
      m_frameCount = 0;
    }
    
    // Update frametime stuff
    m_dataPoints[m_dataPointId] = float(elapsedFtg.count());
    m_dataPointId = (m_dataPointId + 1) % NumDataPoints;
  }
  
  
  HudPos HudFps::render(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    if (m_elements.test(HudElement::Framerate)) {
      position = this->renderFpsText(
        context, renderer, position);
    }
    
    if (m_elements.test(HudElement::Frametimes)) {
      position = this->renderFrametimeGraph(
        context, renderer, position);
    }
    
    return position;
  }
  
  
  HudPos HudFps::renderFpsText(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_fpsString);
    
    return HudPos { position.x, position.y + 24 };
  }
  
  
  HudPos HudFps::renderFrametimeGraph(
    const Rc<DxvkContext>&  context,
          HudRenderer&      renderer,
          HudPos            position) {
    std::array<HudLineVertex, NumDataPoints * 2> vData;
    
    // 60 FPS = optimal, 10 FPS = worst
    const float targetUs =  16'666.6f;
    const float minUs    =   5'000.0f;
    const float maxUs    = 100'000.0f;
    
    // Ten times the maximum/minimum number
    // of milliseconds for a single frame
    uint32_t minMs = 0xFFFFFFFFu;
    uint32_t maxMs = 0x00000000u;
    
    // Paint the time points
    for (uint32_t i = 0; i < NumDataPoints; i++) {
      float us = m_dataPoints[(m_dataPointId + i) % NumDataPoints];
      
      minMs = std::min(minMs, uint32_t(us / 100.0f));
      maxMs = std::max(maxMs, uint32_t(us / 100.0f));
      
      float r = std::min(std::max(-1.0f + us / targetUs, 0.0f), 1.0f);
      float g = std::min(std::max( 3.0f - us / targetUs, 0.0f), 1.0f);
      float l = std::sqrt(r * r + g * g);
      
      HudNormColor color = {
        uint8_t(255.0f * (r / l)),
        uint8_t(255.0f * (g / l)),
        uint8_t(0), uint8_t(255) };
      
      float x = position.x + float(i);
      float y = position.y + 24.0f;
      
      float hVal = std::log2(std::max((us - minUs) / targetUs + 1.0f, 1.0f))
                 / std::log2((maxUs - minUs) / targetUs);
      float h = std::min(std::max(40.0f * hVal, 2.0f), 40.0f);
      
      vData[2 * i + 0] = HudLineVertex { { x, y     }, color };
      vData[2 * i + 1] = HudLineVertex { { x, y - h }, color };
    }
    
    renderer.drawLines(context, vData.size(), vData.data());
    
    // Paint min/max frame times in the entire window
    renderer.drawText(context, 14.0f,
      { position.x, position.y + 44.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format("min: ", minMs / 10, ".", minMs % 10));
    
    renderer.drawText(context, 14.0f,
      { position.x + 150.0f, position.y + 44.0f },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format("max: ", maxMs / 10, ".", maxMs % 10));
    
    return HudPos { position.x, position.y + 66.0f };
  }
  
}