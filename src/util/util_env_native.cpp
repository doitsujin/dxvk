#include "util_env.h"

#include <string>

namespace dxvk::env {

  std::string getEnvVar(const char* name) {
    char* result = std::getenv(name);
    return (result)
      ? result
      : "";
  }
  
  
  std::string getExeName() {
    // TODO
  }
  
  
  void setThreadName(const std::string& name) {
    // TODO
  }

  dxvk_native_info g_native_info;
  
}