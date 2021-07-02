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

  // Convert the given UTF-8 string to a format suitable to pass to functions
  // expecting a filename. On MSVC and MinGW the standard char * variants use
  // the system's code page instead of UTF-8 and there are non-standard
  // overloads which take a wchar_t, however winelib uses the host's libstdc++
  // which doesn't have the wchar_t overload and uses UTF-8.
#ifdef __WINE__
  static inline std::string filename(std::string s) { return s; }
#else
  static inline std::wstring filename(std::string s) { return tows(s.c_str()); }
#endif
  
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
