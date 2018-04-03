#pragma once

#include "../dxvk_stats.h"

#include "dxvk_hud_config.h"
#include "dxvk_hud_text.h"

namespace dxvk::hud {
  
  /**
   * \brief Statistics display for the HUD
   * 
   * Displays some stat counters for the device
   * if enabled. Certain groups of counters can
   * be enabled inidividually.
   */
  class HudStats {
    
  public:
    
    HudStats(HudElements elements);
    ~HudStats();
    
    void update(
      const Rc<DxvkDevice>&   device);
    
    HudPos renderText(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position);
    
  private:
    
    const HudElements m_elements;
    
    DxvkStatCounters  m_prevCounters;
    DxvkStatCounters  m_diffCounters;
    
    HudPos printDrawCallStats(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position);
    
    HudPos printSubmissionStats(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position);
    
    HudPos printPipelineStats(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position);
    
    HudPos printMemoryStats(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position);
    
    static HudElements filterElements(HudElements elements);
    
  };
  
}