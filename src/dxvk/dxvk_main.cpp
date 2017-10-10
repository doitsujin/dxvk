#include "dxvk_main.h"

namespace dxvk {
  
  Logger g_logger("dxvk.log");
  
  Logger* getGlobalLogger() {
    return &g_logger;
  }
  
}