#pragma once

#include "dxvk_hud_text.h"

namespace dxvk::hud {

class HudElement {
public:
    virtual ~HudElement() {} //TODO: rule of 5
    virtual void update() = 0;
    virtual HudPos renderText(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position) = 0;
};

}
