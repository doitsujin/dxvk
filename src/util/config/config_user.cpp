#include <fstream>
#include <sstream>
#include <iostream>

#include "config.h"
#include "config_user.h"

#include "../log/log.h"

namespace dxvk {

  void parseUserConfigLine(Config& config, std::string line);

  const static std::unordered_map<std::string, Config> g_appDefaults = {{
    { "Dishonored2.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    { "F1_2015.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    { "FarCry5.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    { "Frostpunk.exe", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    { "Overwatch.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    { "Wow.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    { "ffxv_s.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    { "mafia3.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
  }};
  
  
  Config getAppConfig(const std::string& appName) {
    auto appConfig = g_appDefaults.find(appName);
    if (appConfig != g_appDefaults.end())
      return appConfig->second;
    return Config();
  }


  Config getUserConfig() {
    Config config;

    // Load either $DXVK_CONFIG_FILE or $PWD/dxvk.conf
    std::string filePath = env::getEnvVar(L"DXVK_CONFIG_FILE");

    if (filePath == "")
      filePath = "dxvk.conf";
    
    // Parse the file line by line
    std::ifstream stream(filePath);
    std::string line;

    while (std::getline(stream, line))
      parseUserConfigLine(config, line);
    
    return config;
  }


  bool isWhitespace(char ch) {
    return ch == ' ' || ch == '\x9' || ch == '\r';
  }

  
  bool isValidKeyChar(char ch) {
    return (ch >= '0' && ch <= '9')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch == '.' || ch == '_');
  }


  size_t skipWhitespace(const std::string& line, size_t n) {
    while (n < line.size() && isWhitespace(line[n]))
      n += 1;
    return n;
  }


  void parseUserConfigLine(Config& config, std::string line) {
    std::stringstream key;
    std::stringstream value;

    // Extract the key
    size_t n = skipWhitespace(line, 0);
    while (n < line.size() && isValidKeyChar(line[n]))
      key << line[n++];
    
    // Check whether the next char is a '='
    n = skipWhitespace(line, n);
    if (n >= line.size() || line[n] != '=')
      return;

    // Extract the value
    n = skipWhitespace(line, n + 1);
    while (n < line.size() && !isWhitespace(line[n]))
      value << line[n++];
    
    config.setOption(key.str(), value.str());
  }

}