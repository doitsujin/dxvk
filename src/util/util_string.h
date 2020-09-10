#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "./com/com_include.h"

namespace dxvk::str {
  
  std::string fromws(const WCHAR *ws);

  void tows(const char* mbs, WCHAR* wcs, size_t wcsLen);

  template <size_t N>
  void tows(const char* mbs, WCHAR (&wcs)[N]) {
    return tows(mbs, wcs, N);
  }

  std::wstring tows(const char* mbs);
  
  inline void format1(std::stringstream&) { }

  template<typename... Tx>
  void format1(std::stringstream& str, const WCHAR *arg, const Tx&... args) {
    str << fromws(arg);
    format1(str, args...);
  }

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
  
}
