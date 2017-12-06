#include "log.h"

#include "../../dxvk/dxvk_main.h"

namespace dxvk {
  
  Logger::Logger(const std::string& file_name)
  : m_fileStream(file_name) { }
  Logger::~Logger() { }
  
  
  void Logger::trace(const std::string& message) {
    s_instance.log(std::string("trace:  ") + message);
  }
  
  
  void Logger::info(const std::string& message) {
    s_instance.log(std::string("info:   ") + message);
  }
  
  
  void Logger::warn(const std::string& message) {
    s_instance.log(std::string("warn:   ") + message);
  }
  
  
  void Logger::err(const std::string& message) {
    s_instance.log(std::string("err:    ") + message);
  }
  
  
  void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::cerr << message << std::endl;
    m_fileStream << message << std::endl;
    m_fileStream.flush();
  }
  
}