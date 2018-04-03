#include "dxvk_hud_cachesize.h"

namespace dxvk::hud {
  
  HudCacheSize::HudCacheSize(const Rc<DxvkPipelineCache>& cache)
  : m_cache(cache),
    m_cacheSizeString("Pipeline Cache Size: ") {
    
  }
  
  
  HudCacheSize::~HudCacheSize() {
    
  }
  
  
  HudPos HudCacheSize::renderText(
    const Rc<DxvkContext>&  context,
          HudTextRenderer&  renderer,
          HudPos            position) {
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_cacheSizeString + str::makeSizeReadable(m_cache->getPipelineCacheSize()) );
    
    return HudPos { position.x, position.y + 20 };
  }
  
}
