#include "util_string.h"

#include <string>
#include <algorithm>

namespace dxvk::str {

  std::string fromws(const WCHAR *ws) {
    size_t count = wcslen(ws);

    return std::string(ws, ws + count);
  }


  void tows(const char* mbs, WCHAR* wcs, size_t wcsLen) {
    std::mbstowcs(wcs, mbs, wcsLen);
  }

}
