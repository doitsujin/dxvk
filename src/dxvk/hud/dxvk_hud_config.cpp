#include "dxvk_hud_config.h"

#include <unordered_map>

namespace dxvk::hud {
  
  const std::unordered_map<std::string, HudElement> g_hudElements = {{
    { "devinfo",      HudElement::DeviceInfo        },
    { "fps",          HudElement::Framerate         },
    { "frametimes",   HudElement::Frametimes        },
    { "drawcalls",    HudElement::StatDrawCalls     },
    { "submissions",  HudElement::StatSubmissions   },
    { "pipelines",    HudElement::StatPipelines     },
    { "memory",       HudElement::StatMemory        },
    { "version",      HudElement::DxvkVersion       },
  }};
  
  
  HudConfig::HudConfig() {
    
  }
  
  
  HudConfig::HudConfig(const std::string& configStr) {
    if (configStr == "1") {
      this->elements.set(
        HudElement::DeviceInfo,
        HudElement::Framerate);
    } else if (configStr == "full") {
      for (auto pair : g_hudElements)
        this->elements.set(pair.second);
    } else {
      std::string::size_type pos = 0;
      std::string::size_type end = 0;
      
      while (pos < configStr.size()) {
        end = configStr.find(',', pos);
        
        if (end == std::string::npos)
          end = configStr.size();
        
        std::string configPart = configStr.substr(pos, end - pos);
        
        auto element = g_hudElements.find(configPart);
        
        if (element != g_hudElements.cend()) {
          this->elements.set(element->second);
          Logger::debug(str::format("Hud: Enabled ", configPart));
        }
        
        pos = end + 1;
      }
    }
  }
  
}