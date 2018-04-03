#pragma once

#include "../dxvk_include.h"

namespace dxvk::hud {
  
  /**
   * \brief HUD element
   * 
   * These flags can be used to enable
   * or disable HUD elements on demand.
   */
  enum class HudElement {
    DeviceInfo        = 0,
    Framerate         = 1,
    StatDrawCalls     = 2,
    StatSubmissions   = 3,
    StatPipelines     = 4,
    StatMemory        = 5,
  };
  
  using HudElements = Flags<HudElement>;
  
  
  /**
   * \brief HUD configuration
   */
  struct HudConfig {
    HudConfig();
    HudConfig(const std::string& configStr);
    
    HudElements elements;
  };
  
  
  /**
   * \brief Gets HUD configuration from config strnig
   * 
   * \param [in] configStr Configuration string
   * \returns HUD configuration struct
   */
  HudConfig parseHudConfigStr(const std::string& configStr);
  
}