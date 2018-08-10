#include <fstream>
#include <sstream>
#include <iostream>

#include "config.h"

#include "../log/log.h"

#include "../util_env.h"

namespace dxvk {

  const static std::unordered_map<std::string, Config> g_appDefaults = {{
    /* Dishonored 2                               */
    { "Dishonored2.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    /* F1 2015                                    */
    { "F1_2015.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    /* Far Cry 5                                  */
    { "FarCry5.exe", {{
      { "d3d11.allowMapFlagNoWait",         "True" }
    }} },
    /* Final Fantasy XV                           */
    { "ffxv_s.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    /* Frostpunk                                  */
    { "Frostpunk.exe", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Just Cause 2 (Dx10)                        */
    { "JustCause2.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    /* Mafia 3                                    */
    { "mafia3.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    /* Overwatch                                  */
    { "Overwatch.exe", {{
      { "d3d11.fakeStreamOutSupport",       "True" },
    }} },
    /* Sleeping Dogs                              */
    { "HKShip.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    /* Sleeping Dogs: Definitive Edition          */
    { "SDHDShip.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    /* Stalker: Call of Pripyat                   */
    { "Stalker-COP.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    /* World of Warcraft                          */
    { "Wow.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    /* World of Warcraft Beta                     */
    { "WowB.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
    /* World of Warcraft Test Branch              */
    { "WowT.exe", {{
      { "dxgi.fakeDx10Support",             "True" },
    }} },
  }};


  static bool isWhitespace(char ch) {
    return ch == ' ' || ch == '\x9' || ch == '\r';
  }

  
  static bool isValidKeyChar(char ch) {
    return (ch >= '0' && ch <= '9')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch == '.' || ch == '_');
  }


  static size_t skipWhitespace(const std::string& line, size_t n) {
    while (n < line.size() && isWhitespace(line[n]))
      n += 1;
    return n;
  }


  static void parseUserConfigLine(Config& config, const std::string& line) {
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


  Config::Config() { }
  Config::~Config() { }


  Config::Config(OptionMap&& options)
  : m_options(std::move(options)) { }


  void Config::merge(const Config& other) {
    for (auto& pair : other.m_options)
      m_options.insert(pair);
  }


  void Config::setOption(const std::string& key, const std::string& value) {
    m_options.insert_or_assign(key, value);
  }


  std::string Config::getOptionValue(const char* option) const {
    auto iter = m_options.find(option);

    return iter != m_options.end()
      ? iter->second : std::string();
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          std::string&  result) {
    result = value;
    return true;
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          bool&         result) {
    if (value == "True") {
      result = true;
      return true;
    } else if (value == "False") {
      result = false;
      return true;
    } else {
      return false;
    }
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          int32_t&      result) {
    if (value.size() == 0)
      return false;
    
    // Parse sign, don't allow '+'
    int32_t sign = 1;
    size_t start = 0;

    if (value[0] == '-') {
      sign = -1;
      start = 1;
    }

    // Parse absolute number
    int32_t intval = 0;

    for (size_t i = start; i < value.size(); i++) {
      if (value[i] < '0' || value[i] > '9')
        return false;
      
      intval *= 10;
      intval += value[i] - '0';
    }

    // Apply sign and return
    result = sign * intval;
    return true;
  }


  Config Config::getAppConfig(const std::string& appName) {
    auto appConfig = g_appDefaults.find(appName);
    if (appConfig != g_appDefaults.end())
      return appConfig->second;
    return Config();
  }


  Config Config::getUserConfig() {
    Config config;

    // Load either $DXVK_CONFIG_FILE or $PWD/dxvk.conf
    std::string filePath = env::getEnvVar(L"DXVK_CONFIG_FILE");

    if (filePath == "")
      filePath = "dxvk.conf";
    
    // Open the file if it exists
    std::ifstream stream(filePath);

    if (!stream)
      return config;

    // Parse the file line by line
    std::string line;

    while (std::getline(stream, line))
      parseUserConfigLine(config, line);
    
    return config;
  }

}