#include "util_env.h"

#include <string>
#include <fstream>

#include <sys/stat.h>
#include <pthread.h>

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

    // Find first null character, arguments are specified after it
    auto f = fullPath.find_first_of('\0');

    fullPath.erase(f, std::string::npos);

    auto n = fullPath.find_last_of('/');
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }
  
  
  void setThreadName(const std::string& name) {
    pthread_setname_np(pthread_self(), name.c_str());
  }

  bool createDirectory(const std::string& path) {
    return !mkdir(path.c_str(), DEFFILEMODE);
  }
  
}