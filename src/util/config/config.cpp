#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

#include "config.h"

#include "../log/log.h"

#include "../util_env.h"

namespace dxvk {

  const static std::vector<std::pair<const char*, Config>> g_appDefaults = {{
    /* Assassin's Creed Syndicate: amdags issues  */
    { R"(\\ACS\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Dissidia Final Fantasy NT Free Edition */
    { R"(\\dffnt\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Elite Dangerous: Compiles weird shaders    *
     * when running on AMD hardware               */
    { R"(\\EliteDangerous64\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Vanishing of Ethan Carter Redux        */
    { R"(\\EthanCarter-Win64-Shipping\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Evil Within: Submits command lists     * 
     * multiple times                             */
    { R"(\\EvilWithin(Demo)?\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Far Cry 3: Assumes clear(0.5) on an UNORM  *
     * format to result in 128 on AMD and 127 on  *
     * Nvidia. We assume that the Vulkan drivers  *
     * match the clear behaviour of D3D11.        */
    { R"(\\(farcry3|fc3_blooddragon)_d3d11\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry 4: Same as Far Cry 3               */
    { R"(\\FarCry4\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry Primal: Nvidia performance         */
    { R"(\\FCPrimal\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    } }},
    /* Frostpunk: Renders one frame with D3D9     *
     * after creating the DXGI swap chain         */
    { R"(\\Frostpunk\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Nioh: See Frostpunk, apparently?           */
    { R"(\\nioh\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Quantum Break: Mever initializes shared    *
     * memory in one of its compute shaders       */
    { R"(\\QuantumBreak\.exe$)", {{
      { "d3d11.zeroInitWorkgroupMemory",    "True" },
    }} },
    /* Anno 2205: Random crashes with state cache */
    { R"(\\anno2205\.exe$)", {{
      { "dxvk.enableStateCache",            "False" },
    }} },
    /* Fifa '19+: Binds typed buffer SRV to shader *
     * that expects raw/structured buffer SRV     */
    { R"(\\FIFA(19|[2-9][0-9])(_demo)?\.exe$)", {{
      { "dxvk.useRawSsbo",                  "True" },
    }} },
    /* Final Fantasy XIV: Fix random black blocks */
    { R"(\\ffxiv_dx11\.exe$)", {{
      { "d3d11.enableRtOutputNanFixup",     "True" },
    }} },
    /* Resident Evil 2/3/7: Ignore WaW hazards    */
    { R"(\\re(2|3|3demo|7|7trial)\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Devil May Cry 5                            */
    { R"(\\DevilMayCry5\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Call of Duty WW2                           */
    { R"(\\s2_sp64_ship\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Need for Speed 2015                        */
    { R"(\\NFS16\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mass Effect Andromeda                      */
    { R"(\\MassEffectAndromeda\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mirror`s Edge Catalyst: Crashes on AMD     */
    { R"(\\MirrorsEdgeCatalyst(Trial)?\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Star Wars Battlefront (2015)               */
    { R"(\\starwarsbattlefront(trial)?\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Dark Souls Remastered                      */
    { R"(\\DarkSoulsRemastered\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Grim Dawn                                  */
    { R"(\\Grim Dawn\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* NieR:Automata                              */
    { R"(\\NieRAutomata\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* SteamVR performance test                   */
    { R"(\\vr\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Hitman 2 - requires AGS library            */
    { R"(\\HITMAN2\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Modern Warfare Remastered                  */
    { R"(\\h1_[ms]p64_ship\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Titan Quest                                */
    { R"(\\TQ\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Saints Row IV                              */
    { R"(\\SaintsRowIV\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
      { "d3d11.invariantPosition",          "True" },
    }} },
    /* Saints Row: The Third                      */
    { R"(\\SaintsRowTheThird_DX11\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
      { "d3d11.invariantPosition",          "True" },
    }} },
    /* Metal Gear Solid 5                         */
    { R"(\\mgsvtpp\.exe$)", {{
      { "dxvk.enableOpenVR",                "False" },
    }} },
    /* Raft                                       */
    { R"(\\Raft\.exe$)", {{
      { "dxvk.enableOpenVR",                "False" },
    }} },
    /* Crysis 3 - slow if it notices AMD card     */
    { R"(\\Crysis3\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Atelier series - games try to render video *
     * with a D3D9 swap chain over the DXGI swap  *
     * chain, which breaks D3D11 presentation     */
    { R"(\\Atelier_(Ayesha|Escha_and_Logy|Shallie)(_EN)?\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Atelier Rorona/Totori/Meruru               */
    { R"(\\A(11R|12V|13V)_x64_Release(_en)?\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Just how many of these games are there?    */
    { R"(\\Atelier_(Lulua|Lydie_and_Suelle|Ryza)\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Star Wars Battlefront II: amdags issues    */
    { R"(\\starwarsbattlefrontii\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Entropia Universe                          */
    { R"(\\Entropia\.exe$)", {{
      { "d3d11.invariantPosition",          "True" },
    }} },
    /* F1 2019 - does not synchronize TGSM access *
     * in a compute shader, causing artifacts     */
    { R"(\\F1_2019\.exe$)", {{
      { "d3d11.forceTgsmBarriers",          "True" },
    }} },
    /* Subnautica                                 */
    { R"(\\Subnautica\.exe$)", {{
      { "dxvk.enableOpenVR",                "False" },
    }} },
    /* Super Monkey Ball: Banana Blitz HD         */
    { R"(\\SMBBBHD\.exe$)", {{
      { "d3d11.enableRtOutputNanFixup",     "True" },
    }} },
    /* Yooka-Laylee and the Impossible Lair       */
    { R"(\\YLILWin64\.exe$)", {{
      { "d3d11.enableRtOutputNanFixup",     "True" },
    }} },
    /* Blue Reflection                            */
    { R"(\\BLUE_REFLECTION\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Secret World Legends                       */
    { R"(\\SecretWorldLegendsDX11\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },

    /**********************************************/
    /* D3D9 GAMES                                 */
    /**********************************************/

    /* A Hat in Time                              */
    { R"(\\HatinTimeGame\.exe$)", {{
      { "d3d9.strictPow",                   "False" },
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Borderlands: The Pre Sequel!               */
    { R"(\\BorderlandsPreSequel\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Borderlands 2                              */
    { R"(\\Borderlands2\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Borderlands                                */
    { R"(\\Borderlands\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Gothic 3                                   */
    { R"(\\Gothic(3|3Final| III Forsaken Gods)\.exe$)", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
      { "d3d9.supportDFFormats",            "False" },
    }} },
    /* Risen                                      */
    { R"(\\Risen[23]?\.exe$)", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
      { "d3d9.invariantPosition",           "True" },
    }} },
    /* Nostale                                    */
    { R"(\\NostaleClientX\.exe$)", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
    }} },
    /* Sonic Adventure 2                          */
    { R"(\\Sonic Adventure 2\\(launcher|sonic2app)\.exe$)", {{
      { "d3d9.floatEmulation",              "False" },
    }} },
    /* The Sims 2,
       Body Shop,
       The Sims Life Stories,
       The Sims Pet Stories,
       and The Sims Castaway Stories             */
    { R"(\\(Sims2.*|TS2BodyShop|SimsLS|SimsPS|SimsCS)\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.customDeviceId",              "0091" },
      { "d3d9.customDeviceDesc",            "GeForce 7800 GTX" },
      { "d3d9.disableA8RT",                 "True" },
      { "d3d9.supportX4R4G4B4",             "False" },
      { "d3d9.maxAvailableMemory",          "2048" },
      { "d3d9.memoryTrackTest",             "True" },
       // The Sims 2 will try to upload 1024 constants
       // every frame otherwise, which it never uses
       // causing a massive discard + upload.
      { "d3d9.swvpFloatCount",              "384" },
      { "d3d9.swvpIntCount",                "16" },
      { "d3d9.swvpBoolCount",               "16" },
    }} },
    /* Dead Space uses the a NULL render target instead
       of a 1x1 one if DF24 is NOT supported      */
    { R"(\\Dead Space\.exe$)", {{
      { "d3d9.supportDFFormats",                 "False" },
    }} },
    /* Burnout Paradise                           */
    { R"(\\BurnoutParadise\.exe$)", {{
      { "d3d9.allowLockFlagReadonly",       "False" },
    }} },
    /* Halo 2                                     */
    { R"(\\halo2\.exe$)", {{
      { "d3d9.invariantPosition",           "True" },
    }} },
    /* Halo CE/HaloPC                             */
    { R"(\\halo(ce)?\.exe$)", {{
      // Game enables minor decal layering fixes
      // specifically when it detects AMD.
      // Avoids chip being detected as unsupported
      // when on intel. Avoids possible path towards
      // invalid texture addressing methods.
      { "d3d9.customVendorId",              "1002" },
      // Avoids card not recognized error.
      // Keeps game's rendering methods consistent
      // for optimal compatibility.
      { "d3d9.customDeviceId",              "4172" },
      // The game uses incorrect sampler types in
      // the shaders for glass rendering which
      // breaks it on native + us if we don't
      // spec-constantly chose the sampler type
      // automagically.
      { "d3d9.forceSamplerTypeSpecConstants", "True" },
    }} },
    /* Counter Strike: Global Offensive
       Needs NVAPI to avoid a forced AO + Smoke
       exploit so we must force AMD vendor ID.    */
    { R"(\\csgo\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
    }} },
    /* Vampire - The Masquerade Bloodlines        */
    { R"(\\vampire\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "1024" },
    }} },
    /* Senran Kagura Shinovi Versus               */
    { R"(\\SKShinoviVersus\.exe$)", {{
      { "d3d9.forceAspectRatio",            "16:9" },
    }} },
    /* Metal Slug X                               */
    { R"(\\mslugx\.exe$)", {{
      { "d3d9.supportD32",                  "False" },
    }} },
    /* Skyrim (NVAPI)                             */
    { R"(\\TESV\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
    }} },
    /* RTHDRIBL Demo                              
       Uses DONOTWAIT after GetRenderTargetData
       then goes into an infinite loop if it gets
       D3DERR_WASSTILLDRAWING.
       This is a better solution than penalizing
       other apps that use this properly.         */
    { R"(\\rthdribl\.exe$)", {{
      { "d3d9.allowDoNotWait",              "False" },
    }} },
    /* Hyperdimension Neptunia U: Action Unleashed */
    { R"(\\Neptunia\.exe$)", {{
      { "d3d9.forceAspectRatio",            "16:9" },
    }} },
    /* D&D - The Temple Of Elemental Evil          */
    { R"(\\ToEE\.exe$)", {{
      { "d3d9.allowDiscard",                "False" },
    }} },
    /* ZUSI 3 - Aerosoft Edition                  */
    { R"(\\ZusiSim\.exe$)", {{
      { "d3d9.noExplicitFrontBuffer",       "True" },
    }} },
    /* GTA IV (NVAPI)                             */
    { R"(\\GTAIV\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
    }} },
    /* Battlefield 2 (bad z-pass)                 */
    { R"(\\BF2\.exe$)", {{
      { "d3d9.longMad",                     "True" },
      { "d3d9.invariantPosition",           "True" },
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
  
  
  bool Config::parseOptionValue(
    const std::string&  value,
          Tristate&     result) {
    if (value == "True") {
      result = Tristate::True;
      return true;
    } else if (value == "False") {
      result = Tristate::False;
      return true;
    } else if (value == "Auto") {
      result = Tristate::Auto;
      return true;
    } else {
      return false;
    }
  }


  Config Config::getAppConfig(const std::string& appName) {
    auto appConfig = std::find_if(g_appDefaults.begin(), g_appDefaults.end(),
      [&appName] (const std::pair<const char*, Config>& pair) {
        std::regex expr(pair.first, std::regex::extended | std::regex::icase);
        return std::regex_search(appName, expr);
      });
    
    if (appConfig != g_appDefaults.end()) {
      // Inform the user that we loaded a default config
      Logger::info(str::format("Found built-in config:"));
      return appConfig->second;
    }

    return Config();
  }


  Config Config::getUserConfig() {
    Config config;

    // Load either $DXVK_CONFIG_FILE or $PWD/dxvk.conf
    std::string filePath = env::getEnvVar("DXVK_CONFIG_FILE");

    if (filePath == "")
      filePath = "dxvk.conf";
    
    // Open the file if it exists
    std::ifstream stream(filePath);

    if (!stream)
      return config;
    
    // Inform the user that we loaded a file, might
    // help when debugging configuration issues
    Logger::info(str::format("Found config file: ", filePath));

    // Initialize parser context
    ConfigContext ctx;
    ctx.active = true;

    // Parse the file line by line
    std::string line;

    while (std::getline(stream, line))
      parseUserConfigLine(config, ctx, line);
    
    return config;
  }


  void Config::logOptions() const {
    if (!m_options.empty()) {
      Logger::info("Effective configuration:");

      for (auto& pair : m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));
    }
  }

}
