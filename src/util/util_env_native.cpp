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
    return "";
  }
  
  
  void setThreadName(const std::string& name) {
    // TODO
  }

  bool createDirectory(const std::string& path) {
    // TODO
    return false;
  }
  
}