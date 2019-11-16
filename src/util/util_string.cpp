#include "util_string.h"

namespace dxvk::str {
  std::string fromws(const WCHAR *ws) {
    size_t len = ::WideCharToMultiByte(CP_UTF8,
      0, ws, -1, nullptr, 0, nullptr, nullptr);

    if (len <= 1)
      return "";

    len -= 1;

    std::string result;
    result.resize(len);
    ::WideCharToMultiByte(CP_UTF8, 0, ws, -1,
      &result.at(0), len, nullptr, nullptr);
    return result;
  }


  void tows(const char* mbs, WCHAR* wcs, size_t wcsLen) {
    ::MultiByteToWideChar(
      CP_UTF8, 0, mbs, -1,
      wcs, wcsLen);
  }

}
