#include "dxvk_hud_dxvkinfo.h"

namespace dxvk::hud {
  
  HudDxvkInfo::HudDxvkInfo() {
  }
  
  
  HudDxvkInfo::~HudDxvkInfo() {
    
  }
  
  
  HudPos HudDxvkInfo::renderText(
    const Rc<DxvkContext>&  context,
          HudTextRenderer&  renderer,
          HudPos            position) {
    renderer.drawText(context, 16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format("DXVK: ", DXVK_VERSION));
    
    return HudPos { position.x, position.y + 24 };
  }

  void HudDxvkInfo::update() {}
  
}
