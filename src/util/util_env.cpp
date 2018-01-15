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
  
}
