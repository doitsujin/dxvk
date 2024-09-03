#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <utility>

#include "config.h"

#include "../log/log.h"

#include "../util_env.h"

namespace dxvk {

  using ProfileList = std::vector<std::pair<const char*, Config>>;


  const static ProfileList g_profiles = {{
    /* 幻想全明星专用 修复黑屏                 */
    { R"(\\ACClient\.exe$)", {{
      { "d3d9.enableDialogMode",        "True" },
	  { "d3d9.maxFrameRate",        "300" },
    }} },
  }};


  const static ProfileList g_deckProfiles = {{
    /* Fallout 4: Defaults to 45 FPS on OLED, but also breaks above 60 FPS */
    { R"(\\Fallout4\.exe$)", {{
      { "dxgi.syncInterval",                "1" },
      { "dxgi.maxFrameRate",                "60" },
    }} },
  }};


  const Config* findProfile(const ProfileList& profiles, const std::string& appName) {
    auto appConfig = std::find_if(profiles.begin(), profiles.end(),
      [&appName] (const std::pair<const char*, Config>& pair) {
        std::regex expr(pair.first, std::regex::extended | std::regex::icase);
        return std::regex_search(appName, expr);
      });

    return appConfig != profiles.end()
      ? &appConfig->second
      : nullptr;
  }


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


  struct ConfigContext {
    bool active;
  };


  static void parseUserConfigLine(Config& config, ConfigContext& ctx, const std::string& line) {
    std::stringstream key;
    std::stringstream value;

    // Extract the key
    size_t n = skipWhitespace(line, 0);

    if (n < line.size() && line[n] == '[') {
      n += 1;

      size_t e = line.size() - 1;
      while (e > n && line[e] != ']')
        e -= 1;

      while (n < e)
        key << line[n++];

      ctx.active = key.str() == env::getExeName();
    } else {
      while (n < line.size() && isValidKeyChar(line[n]))
        key << line[n++];

      // Check whether the next char is a '='
      n = skipWhitespace(line, n);
      if (n >= line.size() || line[n] != '=')
        return;

      // Extract the value
      bool insideString = false;
      n = skipWhitespace(line, n + 1);

      while (n < line.size()) {
        if (!insideString && isWhitespace(line[n]))
          break;

        if (line[n] == '"') {
          insideString = !insideString;
          n++;
        } else
          value << line[n++];
      }

      if (ctx.active)
        config.setOption(key.str(), value.str());
    }
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
    static const std::array<std::pair<const char*, bool>, 2> s_lookup = {{
      { "true",  true  },
      { "false", false },
    }};

    return parseStringOption(value,
      s_lookup.begin(), s_lookup.end(), result);
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


  bool Config::parseOptionValue(
    const std::string&  value,
          float&        result) {
    if (value.size() == 0)
      return false;

    // Parse sign
    size_t pos = 0;
    bool negate = false;

    if (value[0] == '-') {
      negate = true;

      if (++pos == value.size())
        return false;
    }

    // Parse integer part
    uint64_t intPart = 0;

    if (value[pos] == '.')
      return false;

    while (pos < value.size()) {
      if (value[pos] == '.') {
        if (++pos == value.size())
          return false;
        break;
      }

      if (value[pos] < '0' || value[pos] > '9')
        return false;

      intPart *= 10;
      intPart += value[pos] - '0';
      pos += 1;
    }

    // Parse fractional part
    uint64_t fractPart = 0;
    uint64_t fractDivisor = 1;

    while (pos < value.size()) {
      if (value[pos] < '0' || value[pos] > '9')
        return false;

      fractDivisor *= 10;
      fractPart *= 10;
      fractPart += value[pos] - '0';
      pos += 1;
    }

    // Compute final number, not super accurate but this should do
    result = float((double(fractPart) / double(fractDivisor)) + double(intPart));

    if (negate)
      result = -result;

    return std::isfinite(result);
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          Tristate&     result) {
    static const std::array<std::pair<const char*, Tristate>, 3> s_lookup = {{
      { "true",  Tristate::True  },
      { "false", Tristate::False },
      { "auto",  Tristate::Auto  },
    }};

    return parseStringOption(value,
      s_lookup.begin(), s_lookup.end(), result);
  }


  template<typename I, typename V>
  bool Config::parseStringOption(
          std::string   str,
          I             begin,
          I             end,
          V&            value) {
    str = Config::toLower(str);

    for (auto i = begin; i != end; i++) {
      if (str == i->first) {
        value = i->second;
        return true;
      }
    }

    return false;
  }


  Config Config::getAppConfig(const std::string& appName) {
    const Config* config = nullptr;

    if (env::getEnvVar("SteamDeck") == "1")
      config = findProfile(g_deckProfiles, appName);

    if (!config)
      config = findProfile(g_profiles, appName);

    if (config) {
      // Inform the user that we loaded a default config
      Logger::info(str::format("Found built-in config:"));

      for (auto& pair : config->m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));

      return *config;
    }

    return Config();
  }


  Config Config::getUserConfig() {
    Config config;

    // Load either $DXVK_CONFIG_FILE or $PWD/dxvk.conf
    std::string filePath = env::getEnvVar("DXVK_CONFIG_FILE");
    std::string confLine = env::getEnvVar("DXVK_CONFIG");

    if (filePath == "")
      filePath = "dxvk.conf";

    // Open the file if it exists
    std::ifstream stream(str::topath(filePath.c_str()).c_str());

    if (!stream && confLine.empty())
      return config;

    // Initialize parser context
    ConfigContext ctx;
    ctx.active = true;

    if (stream) {
      // Inform the user that we loaded a file, might
      // help when debugging configuration issues
      Logger::info(str::format("加载配置文件: ", filePath));

      // Parse the file line by line
      std::string line;

      while (std::getline(stream, line))
        parseUserConfigLine(config, ctx, line);
    }

    if (!confLine.empty()) {
      // Inform the user that we parsing config from environment, might
      // help when debugging configuration issues
      Logger::info(str::format("Found config env: ", confLine));

      for(auto l : str::split(confLine, ";"))
        parseUserConfigLine(config, ctx, std::string(l.data(), l.size()));
    }

    return config;
  }


  void Config::logOptions() const {
    if (!m_options.empty()) {
      Logger::info("Effective configuration:");

      for (auto& pair : m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));
    }
  }

  std::string Config::toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
      [] (unsigned char c) { return (c >= 'A' && c <= 'Z') ? (c + 'a' - 'A') : c; });
    return str;
  }

}
