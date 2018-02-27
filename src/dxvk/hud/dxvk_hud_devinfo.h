#pragma once

#include "dxvk_hud_element.h"
#include "dxvk_hud_text.h"

namespace dxvk::hud {
  
  /**
   * \brief Device info display for the HUD
   * 
   * Displays the name of the device, as well as
   * the driver version and Vulkan API version.
   */
  class HudDeviceInfo : public HudElement {
    
  public:
    
    HudDeviceInfo(const Rc<DxvkDevice>& device);
    virtual ~HudDeviceInfo();
    
    void update() override;

    HudPos renderText(
      const Rc<DxvkContext>&  context,
            HudTextRenderer&  renderer,
            HudPos            position) override;
    
  private:
    
    std::string m_deviceName;
    std::string m_driverVer;
    std::string m_vulkanVer;
    
  };
  
}
