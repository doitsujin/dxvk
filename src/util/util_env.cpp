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
    std::string fullPath = getExePath();
    auto n = fullPath.find_last_of('\\');
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }


  std::string getExePath() {
    std::vector<WCHAR> exePath;
    exePath.resize(MAX_PATH + 1);

    DWORD len = ::GetModuleFileNameW(NULL, exePath.data(), MAX_PATH);
    exePath.resize(len);

    return str::fromws(exePath.data());
  }
  
  
  void setThreadName(const std::string& name) {
    using SetThreadDescriptionProc = HRESULT (WINAPI *) (HANDLE, PCWSTR);

    static auto proc = reinterpret_cast<SetThreadDescriptionProc>(
      ::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"));

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
