#include "util_string.h"

#ifdef CP_UNIXCP
static constexpr int cp = CP_UNIXCP;
#else
static constexpr int cp = CP_ACP;
#endif

namespace dxvk::str {
  std::string fromws(const WCHAR *ws) {
    size_t len = ::WideCharToMultiByte(cp,
      0, ws, -1, nullptr, 0, nullptr, nullptr);

    if (len <= 1)
      return "";

    len -= 1;

    std::string result;
    result.resize(len);
    ::WideCharToMultiByte(cp, 0, ws, -1,
      &result.at(0), len, nullptr, nullptr);
    return result;
  }
}
