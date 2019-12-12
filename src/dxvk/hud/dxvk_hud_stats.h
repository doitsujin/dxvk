#pragma once

#include "../util/util_time.h"

#include "../dxvk_stats.h"

#include "dxvk_hud_config.h"
#include "dxvk_hud_renderer.h"

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
    
    HudPos render(
            HudRenderer&      renderer,
            HudPos            position);
    
  private:
    
    const HudElements m_elements;
    
    DxvkStatCounters  m_prevCounters;
    DxvkStatCounters  m_diffCounters;

    dxvk::high_resolution_clock::time_point m_gpuLoadUpdateTime;
    dxvk::high_resolution_clock::time_point m_compilerShowTime;

    uint64_t m_prevGpuIdleTicks = 0;
    uint64_t m_diffGpuIdleTicks = 0;
    
    std::string m_gpuLoadString = "GPU: ";

    void updateGpuLoad();
    
    HudPos printDrawCallStats(
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printSubmissionStats(
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printPipelineStats(
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printMemoryStats(
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printGpuLoad(
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printCompilerActivity(
            HudRenderer&      renderer,
            HudPos            position);
    
    static HudElements filterElements(HudElements elements);
    
  };
  
}