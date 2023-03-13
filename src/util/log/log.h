#pragma once

#include <array>
#include <fstream>
#include <iostream>
#include <string>

#include "../thread.h"

namespace dxvk {
  
  enum class LogLevel : uint32_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    None  = 5,
  };
  
  using PFN_wineLogOutput = int (STDMETHODCALLTYPE *)(const char *);

  /**
   * \brief Logger
   * 
   * Logger for one DLL. Creates a text file and
   * writes all log messages to that file.
   */
  class Logger {
    
  public:
    
    Logger(const std::string& file_name);
    ~Logger();
    
    static void trace(const std::string& message);
    static void debug(const std::string& message);
    static void info (const std::string& message);
    static void warn (const std::string& message);
    static void err  (const std::string& message);
    static void log  (LogLevel level, const std::string& message);
    
    static LogLevel logLevel() {
      return s_instance.m_minLevel;
    }
    
  private:
    
    static Logger     s_instance;
    
    const LogLevel    m_minLevel;
    const std::string m_fileName;
    
    dxvk::mutex       m_mutex;
    std::ofstream     m_fileStream;

    bool              m_initialized = false;
    PFN_wineLogOutput m_wineLogOutput = nullptr;

    void emitMsg(LogLevel level, const std::string& message);
    
    std::string getFileName(
      const std::string& base);

    static LogLevel getMinLogLevel();

  };
  
}
