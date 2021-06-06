#include <numeric>

#include "util_env.h"

#include "./com/com_include.h"

namespace dxvk::env {

  std::string getEnvVar(const char* name) {
    std::vector<WCHAR> result;
    result.resize(MAX_PATH + 1);

    DWORD len = ::GetEnvironmentVariableW(str::tows(name).c_str(), result.data(), MAX_PATH);
    result.resize(len);

    return str::fromws(result.data());
  }


  size_t matchFileExtension(const std::string& name, const char* ext) {
    auto pos = name.find_last_of('.');

    if (pos == std::string::npos)
      return pos;

    bool matches = std::accumulate(name.begin() + pos + 1, name.end(), true,
      [&ext] (bool current, char a) {
        if (a >= 'A' && a <= 'Z')
          a += 'a' - 'A';
        return current && *ext && a == *(ext++);
      });

    return matches ? pos : std::string::npos;
  }


  std::string getExeName() {
    std::string fullPath = getExePath();
    auto n = fullPath.find_last_of('\\');
    
    return (n != std::string::npos)
      ? fullPath.substr(n + 1)
      : fullPath;
  }


  std::string getExeBaseName() {
    auto exeName = getExeName();
    auto extp = matchFileExtension(exeName, "exe");

    if (extp != std::string::npos)
      exeName.erase(extp);

    return exeName;
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
      auto wideName = std::vector<WCHAR>(name.length() + 1);
      str::tows(name.c_str(), wideName.data(), wideName.size());
      (*proc)(::GetCurrentThread(), wideName.data());
    }
  }


  bool createDirectory(const std::string& path) {
    WCHAR widePath[MAX_PATH];
    str::tows(path.c_str(), widePath);
    return !!CreateDirectoryW(widePath, nullptr);
  }
  
}
