#include "util_env.h"

#include <unistd.h>
#include <filesystem>

namespace dxvk::env {

  std::string getExePath() {
    std::array<char, 4096> exePath = {};

    size_t count = readlink("/proc/self/exe", exePath.data(), exePath.size());

    return std::string(exePath.begin(), exePath.begin() + count);
  }
  
  
  void setThreadName(const std::string& name) {
  }


  bool createDirectory(const std::string& path) {
    return std::filesystem::create_directories(path);
  }

}