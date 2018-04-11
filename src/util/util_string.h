#pragma once

#include <locale>
#include <codecvt>
#include <string>
#include <sstream>

#ifdef __WINE__
#include "./com/com_include.h"
#endif

namespace dxvk::str {
  
  inline void format1(std::stringstream&) { }
  
  template<typename T, typename... Tx>
  void format1(std::stringstream& str, const T& arg, const Tx&... args) {
    str << arg;
    format1(str, args...);
  }
  
  template<typename... Args>
  std::string format(const Args&... args) {
    std::stringstream stream;
    format1(stream, args...);
    return stream.str();
  }
#ifdef __WINE__
  inline std::string fromws(const std::wstring& ws) {
    char *ret = nullptr;
    if (ws.size() > 0) {
        ret = new char[ws.size() + 1];
        WideCharToMultiByte( CP_UTF8, 0, ws.c_str(), -1, ret, ws.size() + 1, NULL, NULL );
    }
    return ret ? ret : "";
  }
#else
  inline std::string fromws(const std::wstring& ws) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(ws);
  }
#endif
}
