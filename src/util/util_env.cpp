#include "util_env.h"
#include <vector>

#include "./com/com_include.h"

namespace dxvk::env {

  std::string getEnvVar(const wchar_t* name) {
    DWORD len = ::GetEnvironmentVariableW(name, nullptr, 0);
    
    std::vector<WCHAR> result;
    
    while (len > result.size()) {
      result.resize(len);
      len = ::GetEnvironmentVariableW(
        name, result.data(), result.size());
    }
    
    result.resize(len);
    return str::fromws(result.data());
  }
  
  
  std::string getExeName() {
    std::vector<WCHAR> exePath;
    exePath.resize(MAX_PATH + 1);
    
    DWORD len = ::GetModuleFileNameW(NULL, exePath.data(), MAX_PATH);
    exePath.resize(len);
    
    std::string fullPath = str::fromws(exePath.data());
    auto n = fullPath.find_last_of('\\');
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }
  
  
  void setThreadName(const wchar_t* name) {
    using SetThreadDescriptionProc = void (WINAPI *) (HANDLE, PCWSTR);

    HMODULE module = ::GetModuleHandleW(L"kernel32.dll");

    if (module == nullptr)
      return;

    auto proc = reinterpret_cast<SetThreadDescriptionProc>(
      ::GetProcAddress(module, "SetThreadDescription"));

    if (proc != nullptr)
      (*proc)(::GetCurrentThread(), name);
  }
  
}
