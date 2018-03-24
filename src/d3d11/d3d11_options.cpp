#include <unordered_map>

#include "d3d11_options.h"

namespace dxvk {
  
  const static std::unordered_map<std::string, D3D11OptionSet> g_d3d11AppOptions = {{
    { "Dishonored2.exe", D3D11OptionSet(D3D11Option::AllowMapFlagNoWait) },
  }};
  
  
  D3D11OptionSet D3D11GetAppOptions(const std::string& AppName) {
    auto appOptions = g_d3d11AppOptions.find(AppName);
    
    return appOptions != g_d3d11AppOptions.end()
      ? appOptions->second
      : D3D11OptionSet();
  }
  
}