#include "dxgi_options.h"

#include <unordered_map>

namespace dxvk {
  
  const static std::unordered_map<std::string, DxgiOptions> g_dxgiAppOptions = {{
    { "Frostpunk.exe", DxgiOptions(DxgiOption::DeferSurfaceCreation) },
  }};
  
  
  DxgiOptions getDxgiAppOptions(const std::string& appName) {
    auto appOptions = g_dxgiAppOptions.find(appName);
    
    return appOptions != g_dxgiAppOptions.end()
      ? appOptions->second
      : DxgiOptions();
  }
  
}