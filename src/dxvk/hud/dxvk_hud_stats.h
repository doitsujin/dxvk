#pragma once

#include <chrono>

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
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
  private:
    
    const HudElements m_elements;
    
    DxvkStatCounters  m_prevCounters;
    DxvkStatCounters  m_diffCounters;

    std::chrono::high_resolution_clock::time_point m_gpuLoadUpdateTime;
    std::chrono::high_resolution_clock::time_point m_compilerShowTime;

    uint64_t m_prevGpuIdleTicks = 0;
    uint64_t m_diffGpuIdleTicks = 0;
    
    std::string m_gpuLoadString = "GPU: ";

    void updateGpuLoad();
    
    HudPos printDrawCallStats(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printSubmissionStats(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printPipelineStats(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printMemoryStats(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printGpuLoad(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    HudPos printCompilerActivity(
      const Rc<DxvkContext>&  context,
            HudRenderer&      renderer,
            HudPos            position);
    
    static HudElements filterElements(HudElements elements);
    
  };
  
}