#include <unordered_map>

#include "dxvk_options.h"

namespace dxvk {
  
  const static std::unordered_map<std::string, DxvkOptionSet> g_appOptions = {{
    
  }};
  
  
  void DxvkOptions::adjustAppOptions(const std::string& appName) {
    auto appOptions = g_appOptions.find(appName);
    
    if (appOptions != g_appOptions.end())
      m_options.set(appOptions->second);
  }
  
  
  void DxvkOptions::adjustDeviceOptions(const Rc<DxvkAdapter>& adapter) {
    
  }
  
  
  void DxvkOptions::logOptions() const {
    #define LOG_OPTION(opt) this->logOption(DxvkOption::opt, #opt)
    LOG_OPTION(AssumeNoZfight);
    #undef LOG_OPTION
  }
  
  
  void DxvkOptions::logOption(
          DxvkOption          option,
          const std::string&  name) const {
    if (m_options.test(option))
      Logger::info(str::format("Using option ", name));
  }
  
}