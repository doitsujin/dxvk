#include "util_string.h"
#include "util_utf.h"

namespace dxvk::str {

  template <typename FromChar, typename ToChar>
  size_t reencodeString(const FromChar* from, ToChar* to, size_t toLen = 0) {
    using UTFFromChar = typename utf::UTFTypeTraits<FromChar>::CharType;
    using UTFToChar   = typename utf::UTFTypeTraits<ToChar>::CharType;

    const UTFFromChar* utfFrom = reinterpret_cast<const UTFFromChar*>(from);
    UTFToChar* utfTo = reinterpret_cast<UTFToChar*>(to);

    size_t totalSize = 0;
    while (*from) {
      char32_t utf32char = utf::decodeUTF<UTFFromChar>(&utfFrom);
      size_t nextCharSize = utf::encodeUTF<UTFToChar>(utf32char, nullptr);
      if (utfTo) {
        if (toLen && totalSize + nextCharSize > toLen)
          return totalSize;

        utf::encodeUTF<UTFToChar>(utf32char, utfTo);
      }

      totalSize += nextCharSize;
    }

    return totalSize;
  }

  std::string fromws(const WCHAR* ws) {
    size_t len = reencodeString<WCHAR, char>(ws, nullptr);

    std::string str;
    str.resize(len);
    reencodeString<WCHAR, char>(ws, str.data());

    return str;
  }

  void tows(const char* mbs, WCHAR* wcs, size_t wcsLen) {
    reencodeString(mbs, wcs, wcsLen);
  }

  std::wstring tows(const char* mbs) {
    size_t len = reencodeString<char, wchar_t>(mbs, nullptr);

    std::wstring str;
    str.resize(len);
    reencodeString<char, wchar_t>(mbs, str.data());

    return str;
  }

}
