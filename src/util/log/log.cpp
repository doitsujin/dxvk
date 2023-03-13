#include <utility>

#include "log.h"

#include "../util_env.h"

namespace dxvk {
  
  Logger::Logger(const std::string& fileName)
  : m_minLevel(getMinLogLevel()), m_fileName(fileName) {

  }
  
  
  Logger::~Logger() { }
  
  
  void Logger::trace(const std::string& message) {
    s_instance.emitMsg(LogLevel::Trace, message);
  }
  
  
  void Logger::debug(const std::string& message) {
    s_instance.emitMsg(LogLevel::Debug, message);
  }
  
  
  void Logger::info(const std::string& message) {
    s_instance.emitMsg(LogLevel::Info, message);
  }
  
  
  void Logger::warn(const std::string& message) {
    s_instance.emitMsg(LogLevel::Warn, message);
  }
  
  
  void Logger::err(const std::string& message) {
    s_instance.emitMsg(LogLevel::Error, message);
  }
  
  
  void Logger::log(LogLevel level, const std::string& message) {
    s_instance.emitMsg(level, message);
  }
  
  
  void Logger::emitMsg(LogLevel level, const std::string& message) {
    if (level >= m_minLevel) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      
      static std::array<const char*, 5> s_prefixes
        = {{ "trace: ", "debug: ", "info:  ", "warn:  ", "err:   " }};
      
      const char* prefix = s_prefixes.at(static_cast<uint32_t>(level));

      if (!std::exchange(m_initialized, true)) {
#ifdef _WIN32
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");

        if (ntdll)
          m_wineLogOutput = reinterpret_cast<PFN_wineLogOutput>(GetProcAddress(ntdll, "__wine_dbg_output"));
#endif
        auto path = getFileName(m_fileName);

        if (!path.empty())
          m_fileStream = std::ofstream(str::topath(path.c_str()).c_str());
      }

      std::stringstream stream(message);
      std::string line;

      while (std::getline(stream, line, '\n')) {
        std::stringstream outstream;
        outstream << prefix << line << std::endl;

        std::string adjusted = outstream.str();

        if (!adjusted.empty()) {
          if (m_wineLogOutput)
            m_wineLogOutput(adjusted.c_str());
          else
            std::cerr << adjusted;
        }

        if (m_fileStream)
          m_fileStream << adjusted;
      }
    }
  }
  
  
  std::string Logger::getFileName(const std::string& base) {
    std::string path = env::getEnvVar("DXVK_LOG_PATH");
    
    if (path == "none")
      return std::string();

    // Don't create a log file if we're writing to wine's console output
    if (path.empty() && m_wineLogOutput)
      return std::string();

    if (!path.empty() && *path.rbegin() != '/')
      path += '/';

    std::string exeName = env::getExeBaseName();
    path += exeName + "_" + base;
    return path;
  }


  LogLevel Logger::getMinLogLevel() {
    const std::array<std::pair<const char*, LogLevel>, 6> logLevels = {{
      { "trace", LogLevel::Trace },
      { "debug", LogLevel::Debug },
      { "info",  LogLevel::Info  },
      { "warn",  LogLevel::Warn  },
      { "error", LogLevel::Error },
      { "none",  LogLevel::None  },
    }};
    
    const std::string logLevelStr = env::getEnvVar("DXVK_LOG_LEVEL");
    
    for (const auto& pair : logLevels) {
      if (logLevelStr == pair.first)
        return pair.second;
    }
    
    return LogLevel::Info;
  }
  
}
