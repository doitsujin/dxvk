#include "d3d9_cursor.h"
#include "d3d9_util.h"

#include <utility>

namespace dxvk {

#ifdef _WIN32
  void D3D9Cursor::UpdateCursor(int X, int Y) {
    POINT currentPos = { };
    if (::GetCursorPos(&currentPos) && currentPos == POINT{ X, Y })
        return;

    ::SetCursorPos(X, Y);
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    if (likely(m_hCursor != nullptr))
      ::SetCursor(bShow ? m_hCursor : nullptr);
    else
      Logger::debug("D3D9Cursor::ShowCursor: Software cursor not implemented.");
    
    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(
          UINT                   XHotSpot,
          UINT                   YHotSpot,
    const std::vector<uint8_t>&  bitmap,
          bool                   cursorEmulation,
          UINT                   width,
          UINT                   height,
          HWND                   window) {
    bool noPreviousHardwareCursor = false;

    uint32_t cursorWidth = cursorEmulation ? width : HardwareCursorWidth;
    uint32_t cursorHeight = cursorEmulation ? height : HardwareCursorHeight;

    // For color icons, the hbmMask and hbmColor bitmaps
    // are the same size, each of which is the size of the icon.
    std::vector<DWORD> mask(cursorWidth * cursorHeight, ~0);

    ICONINFO info;
    info.fIcon    = FALSE;
    info.xHotspot = XHotSpot;
    info.yHotspot = YHotSpot;
    info.hbmMask  = ::CreateBitmap(cursorWidth, cursorHeight, 1, 1,  &mask[0]);
    info.hbmColor = ::CreateBitmap(cursorWidth, cursorHeight, 1, 32, &bitmap[0]);

    if (m_hCursor != nullptr)
      ::DestroyCursor(m_hCursor);
    else
      noPreviousHardwareCursor = true;

    m_hCursor = ::CreateIconIndirect(&info);

    ::DeleteObject(info.hbmMask);
    ::DeleteObject(info.hbmColor);

    if (cursorEmulation) {
      ::SetClassLongPtr(window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(m_hCursor));
      
      CURSORINFO ci;
      while (::GetCursorInfo(&ci) && ci.flags == 0u)
        ::ShowCursor(TRUE);

      // Castle Strike needs one extra initial increment
      // to display the emulated cursor on its menu
      if (noPreviousHardwareCursor)
        ::ShowCursor(TRUE);
    }

    ShowCursor(m_visible);

    return D3D_OK;
  }
#else
  void D3D9Cursor::UpdateCursor(int X, int Y) {
    Logger::warn("D3D9Cursor::UpdateCursor: Not supported on current platform.");
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    Logger::warn("D3D9Cursor::ShowCursor: Not supported on current platform.");
    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(
          UINT                   XHotSpot,
          UINT                   YHotSpot,
    const std::vector<uint8_t>&  bitmap,
          bool                   cursorEmulation,
          UINT                   width,
          UINT                   height,
          HWND                   window) {
    Logger::warn("D3D9Cursor::SetHardwareCursor: Not supported on current platform.");

    return D3D_OK;
  }
#endif

}
