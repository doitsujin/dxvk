#include "log.h"

namespace dxvk {
  
  Log::Log(const std::string& filename)
  : m_stream(filename, std::ios_base::out | std::ios_base::trunc) {
    
  }
  
  
  Log::~Log() {
    
  }
  
  
  void Log::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << message << std::endl;
    std::cerr.flush();
    
    m_stream  << message << std::endl;
    m_stream.flush();
  }
  
}