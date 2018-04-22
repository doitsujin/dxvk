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
  inline std::string fromws(const wchar_t* ws) {
    std::string ret;
    DWORD len = WideCharToMultiByte( CP_UNIXCP, 0, ws, -1, NULL, 0, NULL, NULL );
    ret.resize(len);
    WideCharToMultiByte( CP_UNIXCP, 0, ws, -1, &ret.at(0), len, NULL, NULL );
    ret.resize(ret.length() - 1); // drop '\0'
    return ret;
  }
#else
  inline std::string fromws(const wchar_t* ws) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(ws);
  }
#endif
}
