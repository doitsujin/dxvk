#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace dxvk {
  
  enum class LogLevel : uint32_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    None  = 5,
  };
  
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
    
  private:
    
    static Logger s_instance;
    
    const LogLevel m_minLevel;
    
    std::mutex    m_mutex;
    std::ofstream m_fileStream;
    
    void log(LogLevel level, const std::string& message);
    
    static LogLevel getMinLogLevel();

  };
  
}
