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
    Frametimes        = 2,
    StatDrawCalls     = 3,
    StatSubmissions   = 4,
    StatPipelines     = 5,
    StatMemory        = 6,
    DxvkVersion       = 7,
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