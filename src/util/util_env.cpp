#include "util_env.h"

namespace dxvk::env {

#ifndef DXVK_NATIVE
  constexpr char dirSlash = '\\';
#else
  constexpr char dirSlash = '/';
#endif


  std::string getEnvVar(const char* name) {
    char* result = std::getenv(name);
    return (result)
      ? result
      : "";
  }
  
  
  std::string getExeName() {
    std::string fullPath = getExePath();
    auto n = fullPath.find_last_of(dirSlash);
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }
  
}
