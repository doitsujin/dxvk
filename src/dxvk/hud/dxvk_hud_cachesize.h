#pragma once

#include "dxvk_hud_text.h"

#include "../dxvk_pipecache.h"
#include "../util/util_string.h"

namespace dxvk::hud {
    
  /**
   * \brief Pipeline cache size display for the HUD
   * 
   * Displays the current size of the cache.
   */
  class HudCacheSize {
  public:
    
    HudCacheSize(const Rc<DxvkPipelineCache>& cache);
    ~HudCacheSize();
    
    HudPos renderText(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position);
    
  private:
    
    Rc<DxvkPipelineCache> m_cache;
    
    std::string m_cacheSizeString;
    
  };
  
}
