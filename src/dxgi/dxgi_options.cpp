#include "dxgi_options.h"

#include <unordered_map>

namespace dxvk {
  
  const static std::unordered_map<std::string, DxgiOptions> g_dxgiAppOptions = {{
    { "Frostpunk.exe", DxgiOptions(DxgiOption::DeferSurfaceCreation) },
    { "Wow.exe",       DxgiOptions(DxgiOption::FakeDx10Support)      },
  }};
  
  
  DxgiOptions getDxgiAppOptions(const std::string& appName) {
    DxgiOptions options;

    auto appOptions = g_dxgiAppOptions.find(appName);
    if (appOptions != g_dxgiAppOptions.end())
      options = appOptions->second;
    
    if (env::getEnvVar(L"DXVK_FAKE_DX10_SUPPORT") == "1")
      options.set(DxgiOption::FakeDx10Support);
    
    return options;
  }
  
}