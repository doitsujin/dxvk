#pragma once

#include "util_string.h"

namespace dxvk::utf {

  using utf8_char  = uint8_t;
  using utf16_char = uint16_t;
  using utf32_char = uint32_t;
  
  // https://en.wikipedia.org/wiki/Specials_(Unicode_block)#Replacement_character
  static constexpr utf32_char UTF32_REPLACEMENT_CHARACTER = 0xFFFD;

  bool isValidUTF32Char(utf32_char utf32char) {
    return ((utf32char < 0x110000u)) &&
           ((utf32char - 0x00D800u) > 0x7FFu) &&
           ((utf32char & 0xFFFFu) < 0xFFFEu) &&
           ((utf32char - 0x00FDD0u) > 0x1Fu);
  }

  // Decoding

  template <typename T>
  static utf32_char decodeUTF(const T** strPtr);

  template <>
  utf32_char decodeUTF<utf8_char>(const utf8_char** strPtr) {
    const utf8_char *str = *strPtr;

    utf8_char headerChar = (utf8_char)*str;
    size_t size;
    if (headerChar == 0)
      size = 0;
    else if ((headerChar & 0x80) == 0)
      size = 1;
    else if ((headerChar & 0xE0) == 0xC0)
      size = 2;
    else if ((headerChar & 0xF0) == 0xE0)
      size = 3;
    else if ((headerChar & 0xF8) == 0xF0)
      size = 4;
    else
      size = 0;

    if (size == 0) {
      // Not sure how to handle invalid UTF8 here.
      // Just return a replacement character and advance.
      *strPtr += 1;
      return UTF32_REPLACEMENT_CHARACTER;
    }

    *strPtr += size;

    // This below was taken from Gamescope, I need to validate it.
    // I don't trust this at all, it seems way too simple...
    // (Pairs?)
    constexpr utf32_char masks[] = { 0x7F, 0x1F, 0x0F, 0x07 };
    utf32_char ret = utf32_char(*str) & masks[size - 1];
    for (size_t i = 1; i < size; i++) {
      ret <<= 6;
      ret |= str[i] & 0x3F;
    }
    
    if (isValidUTF32Char(ret))
      return ret;
    
    return UTF32_REPLACEMENT_CHARACTER;
  }

  template <>
  utf32_char decodeUTF<utf16_char>(const utf16_char** strPtr) {
    const utf16_char *str = *strPtr;
    // If this is a valid UTF32 character, go straight ahead,
    // otherwise, check if this is a pair encoding.
    if (isValidUTF32Char(*str))
      return utf32_char(*(*strPtr++));
    else if ((str[0] - 0xD800u) < 0x400u && (str[1] - 0xDC00u) < 0x400u) {
      *strPtr += 2;
      utf32_char utf32char = 0x010000 + ((str[0] - 0xD800u) << 10) +
                             (str[1] - 0xDC00);
      if (isValidUTF32Char(utf32char))
        return utf32char;
    }
    
    return UTF32_REPLACEMENT_CHARACTER;
  }

  template <>
  utf32_char decodeUTF<utf32_char>(const utf32_char** strPtr) {
    const utf32_char *str = *strPtr;
    *strPtr += 1;

    if (isValidUTF32Char(*str))
      return *str;

    return UTF32_REPLACEMENT_CHARACTER;
  }

  // Encoding

  template <typename T>
  static size_t encodeUTF(utf32_char utf32char, T* out);

  template <>
  size_t encodeUTF<utf32_char>(utf32_char utf32char, utf32_char* out) {
    if (out)
      *out = utf32char;
    return 1;
  }

  template <>
  size_t encodeUTF<utf16_char>(utf32_char utf32char, utf16_char* out) {
    if (utf32char <= 0xFFFF) {
      if (out)
        out[0] = utf16_char(utf32char);
      return 1;
    } else {
      if (out) {
        out[0] = utf16_char((utf32char - 0x010000) >> 10)   | 0xD800;
        out[1] = utf16_char((utf32char - 0x010000) & 0x3FF) | 0xDC00;
      }
      return 2;
    }
  }

  template <>
  size_t encodeUTF<utf8_char>(utf32_char utf32char, utf8_char* out) {
    if (utf32char <= 0x7F) {
      if (out)
        out[0] = utf8_char(utf32char);
      return 1;
    }
    else if (utf32char <= 0x7FF) {
      if (out) {
        out[0] = utf8_char((utf32char >> 6))        | 0xC0;
        out[1] = utf8_char((utf32char >> 0) & 0x3F) | 0x80;
      }
      return 2;
    }
    else if (utf32char <= 0xFFFF) {
      if (out) {
        out[0] = utf8_char((utf32char >> 12))       | 0xE0;
        out[1] = utf8_char((utf32char >> 6) & 0x3F) | 0x80;
        out[2] = utf8_char((utf32char >> 0) & 0x3F) | 0x80;
      }
      return 3;
    } else {
      if (out) {
        out[0] = utf8_char((utf32char >> 18) & 0x07) | 0xF0;
        out[1] = utf8_char((utf32char >> 12) & 0x3F) | 0x80;
        out[2] = utf8_char((utf32char >> 6)  & 0x3F) | 0x80;
        out[3] = utf8_char((utf32char >> 0)  & 0x3F) | 0x80;
      }
      return 4;
    }
  }

  template <size_t Size>
  struct UTFTypeTraits_;

  template <> struct UTFTypeTraits_<1> { using CharType = utf8_char; };
  template <> struct UTFTypeTraits_<2> { using CharType = utf16_char; };
  template <> struct UTFTypeTraits_<4> { using CharType = utf32_char; };

  template <typename T>
  struct UTFTypeTraits : public UTFTypeTraits_<sizeof(T)> {};

}