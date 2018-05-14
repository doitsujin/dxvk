#pragma once

#include <locale>
#include <codecvt>
#include <string>
#include <sstream>

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

  inline std::string fromws(const std::wstring& ws) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(ws);
  }

}
