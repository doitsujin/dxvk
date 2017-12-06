#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

#include "../rc/util_rc.h"

namespace dxvk {
  
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
    static void info (const std::string& message);
    static void warn (const std::string& message);
    static void err  (const std::string& message);
    
  private:
    
    static Logger s_instance;
    
    std::mutex    m_mutex;
    std::ofstream m_fileStream;
    
    void log(const std::string& message);

  };
  
}