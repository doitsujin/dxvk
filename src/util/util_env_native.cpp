#include "util_env.h"

#include <string>
#include <fstream>
#include <sys/stat.h>

namespace dxvk::env {

  std::string getEnvVar(const char* name) {
    char* result = std::getenv(name);
    return (result)
      ? result
      : "";
  }
  
  
  std::string getExeName() {
    std::string fullPath;

    std::ifstream cmdLineFile;
    cmdLineFile.open("/proc/self/cmdline");
      if(cmdLineFile.good())
        std::getline(cmdLineFile, fullPath);
    cmdLineFile.close();

    auto n = fullPath.find_last_of('/');
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }
  
  
  void setThreadName(const std::string& name) {
    // TODO
  }

  bool createDirectory(const std::string& path) {
    return !mkdir(path.c_str(), DEFFILEMODE);
  }
  
}