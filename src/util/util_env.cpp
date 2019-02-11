#include "util_env.h"

#include "./com/com_include.h"

namespace dxvk::env {

  std::string getEnvVar(const char* name) {
    char* result = std::getenv(name);
    return (result)
      ? result
      : "";
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
  
  
  void setThreadName(const std::string& name) {
    using SetThreadDescriptionProc = void (WINAPI *) (HANDLE, PCWSTR);

    HMODULE module = ::GetModuleHandleW(L"kernel32.dll");

    if (module == nullptr)
      return;

    auto proc = reinterpret_cast<SetThreadDescriptionProc>(
      ::GetProcAddress(module, "SetThreadDescription"));

    if (proc != nullptr) {
      auto wideName = str::tows(name);
      (*proc)(::GetCurrentThread(), wideName.data());
    }
  }


  bool createDirectory(const std::string& path) {
    auto widePath = str::tows(path);
    return !!CreateDirectoryW(widePath.data(), nullptr);
  }
  
}
