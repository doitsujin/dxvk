#include "util_env.h"

#include "./com/com_include.h"

namespace dxvk::env {

  std::string getEnvVar(const wchar_t* name) {
    DWORD len = ::GetEnvironmentVariableW(name, nullptr, 0);
    
    std::wstring result;
    
    while (len > result.size()) {
      result.resize(len);
      len = ::GetEnvironmentVariableW(
        name, &result.at(0), result.size());
    }
    
    result.resize(len);
    return str::fromws(result);
  }
  
  
  std::string getExeName() {
    std::wstring exePath;
    exePath.resize(MAX_PATH + 1);
    
    DWORD len = ::GetModuleFileNameW(NULL, &exePath.at(0), MAX_PATH);
    exePath.resize(len);
    
    std::string fullPath = str::fromws(exePath);
    auto n = fullPath.find_last_of('\\');
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }
  
}
