#pragma once

#include "d3d9_include.h"

namespace dxvk {

  /**
   * \brief D3D9 Software Cursor
   */
  struct D3D9_SOFTWARE_CURSOR {
    UINT Width  = 0;
    UINT Height = 0;
    UINT XHotSpot = 0;
    UINT YHotSpot = 0;
    int32_t X = 0;
    int32_t Y = 0;
    bool DrawCursor  = false;
    bool ResetCursor = false;
  };

  constexpr uint32_t HardwareCursorWidth      = 32u;
  constexpr uint32_t HardwareCursorHeight     = 32u;
  constexpr uint32_t HardwareCursorFormatSize = 4u;
  constexpr uint32_t HardwareCursorPitch      = HardwareCursorWidth * HardwareCursorFormatSize;

  // Format Size of 4 bytes (ARGB)
  using CursorBitmap = uint8_t[HardwareCursorHeight * HardwareCursorPitch];
  // Monochrome mask (1 bit)
  using CursorMask   = uint8_t[HardwareCursorHeight * HardwareCursorWidth / 8];

  class D3D9Cursor {

  public:

#ifdef _WIN32
    ~D3D9Cursor() {
      if (m_hCursor != nullptr)
        ::DestroyCursor(m_hCursor);
    }
#endif

    void ResetCursor();

    void ResetHardwareCursor();

    void ResetSoftwareCursor();

    void UpdateCursor(int X, int Y);

    BOOL ShowCursor(BOOL bShow);

    HRESULT SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap);

    HRESULT SetSoftwareCursor(UINT Width, UINT Height, UINT XHotSpot, UINT YHotSpot);

    D3D9_SOFTWARE_CURSOR* GetSoftwareCursor() {
      return &m_sCursor;
    }

    BOOL IsSoftwareCursor() const {
      return m_sCursor.Width > 0 && m_sCursor.Height > 0;
    }

    BOOL IsCursorVisible() const {
      return m_visible;
    }

  private:

    BOOL                  m_visible   = FALSE;
    D3D9_SOFTWARE_CURSOR  m_sCursor;

#ifdef _WIN32
    HCURSOR               m_hCursor   = nullptr;
#endif

  };

}