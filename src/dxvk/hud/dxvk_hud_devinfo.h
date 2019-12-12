#pragma once

#include "dxvk_hud_renderer.h"

namespace dxvk::hud {
  
  /**
   * \brief Device info display for the HUD
   * 
   * Displays the name of the device, as well as
   * the driver version and Vulkan API version.
   */
  class HudDeviceInfo {
    
  public:
    
    HudDeviceInfo(const Rc<DxvkDevice>& device);
    ~HudDeviceInfo();
    
    HudPos render(
            HudRenderer&      renderer,
            HudPos            position);
    
  private:
    
    std::string m_deviceName;
    std::string m_driverVer;
    std::string m_vulkanVer;
    
  };
  
}