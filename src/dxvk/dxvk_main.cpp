#include "dxvk_main.h"

namespace dxvk {
  
  Log g_logger("dxvk.log");
  
  void log(const std::string& message) {
    g_logger.log(message);
  }
  
}