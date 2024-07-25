#pragma once

#include "d3d9_include.h"

namespace dxvk {

  constexpr uint32_t HardwareCursorWidth     = 32u;
  constexpr uint32_t HardwareCursorHeight    = 32u;
  constexpr uint32_t HardwareCursorFormatSize = 4u;
  constexpr uint32_t HardwareCursorPitch      = HardwareCursorWidth * HardwareCursorFormatSize;

  // Format Size of 4 bytes (ARGB)
  using CursorBitmap = uint8_t[HardwareCursorHeight * HardwareCursorPitch];
  // Monochrome mask (1 bit)
  using CursorMask = uint8_t[HardwareCursorHeight * HardwareCursorWidth / 8];

  class D3D9Cursor {

  public:

#ifdef _WIN32
    ~D3D9Cursor() {
      if (m_hCursor != nullptr)
        ::DestroyCursor(m_hCursor);
    }
#endif

    void UpdateCursor(int X, int Y);

    BOOL ShowCursor(BOOL bShow);

    HRESULT SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap);

  private:

    BOOL    m_visible       = FALSE;

#ifdef _WIN32
    HCURSOR m_hCursor       = nullptr;
#endif

  };

}