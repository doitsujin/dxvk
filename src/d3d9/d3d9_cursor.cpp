#include "d3d9_cursor.h"
#include "d3d9_util.h"

#include <utility>

namespace dxvk {

#ifdef _WIN32
  void D3D9Cursor::ResetCursor() {
    ShowCursor(FALSE);

    if (m_hCursor != nullptr)
      ResetHardwareCursor();
    else if (IsSoftwareCursor())
      ResetSoftwareCursor();
  }


  void D3D9Cursor::ResetHardwareCursor() {
    ::DestroyCursor(m_hCursor);
    m_hCursor = nullptr;
  }


  void D3D9Cursor::ResetSoftwareCursor() {
    m_sCursor.DrawCursor = false;
    m_sCursor.ResetCursor = true;
  }


  void D3D9Cursor::UpdateCursor(int X, int Y) {
    // SetCursorPosition is used to directly update the position of software cursors,
    // but keep track of the cursor position even when using hardware cursors, in order
    // to ensure a smooth transition/overlap from one type to the other.
    m_sCursor.X = X;
    m_sCursor.Y = Y;

    if (unlikely(m_sCursor.Width > 0 && m_sCursor.Height > 0))
      return;

    POINT currentPos = { };
    if (::GetCursorPos(&currentPos) && currentPos == POINT{ X, Y })
        return;

    ::SetCursorPos(X, Y);
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    // Cursor visibility remains unchanged (typically FALSE) if the cursor isn't set.
    if (unlikely(m_hCursor == nullptr && !IsSoftwareCursor()))
      return m_visible;

    if (m_hCursor != nullptr)
      ::SetCursor(bShow ? m_hCursor : nullptr);
    else if (likely(!m_sCursor.ResetCursor))
      m_sCursor.DrawCursor = bShow;
    
    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    if (IsSoftwareCursor())
      ResetSoftwareCursor();

    CursorMask mask;
    std::memset(mask, ~0, sizeof(mask));

    ICONINFO info;
    info.fIcon    = FALSE;
    info.xHotspot = XHotSpot;
    info.yHotspot = YHotSpot;
    info.hbmMask  = ::CreateBitmap(HardwareCursorWidth, HardwareCursorHeight, 1, 1,  &mask[0]);
    info.hbmColor = ::CreateBitmap(HardwareCursorWidth, HardwareCursorHeight, 1, 32, &bitmap[0]);

    if (m_hCursor != nullptr)
      ::DestroyCursor(m_hCursor);

    m_hCursor = ::CreateIconIndirect(&info);

    ::DeleteObject(info.hbmMask);
    ::DeleteObject(info.hbmColor);

    ShowCursor(m_visible);

    return D3D_OK;
  }


  HRESULT D3D9Cursor::SetSoftwareCursor(UINT Width, UINT Height, UINT XHotSpot, UINT YHotSpot) {
    // Make sure to hide the win32 cursor
    ::SetCursor(nullptr);

    if (m_hCursor != nullptr)
      ResetHardwareCursor();

    m_sCursor.Width       = Width;
    m_sCursor.Height      = Height;
    m_sCursor.XHotSpot    = XHotSpot;
    m_sCursor.YHotSpot    = YHotSpot;
    m_sCursor.ResetCursor = false;

    ShowCursor(m_visible);

    return D3D_OK;
  }

#else
  void D3D9Cursor::ResetCursor() {
    Logger::warn("D3D9Cursor::ResetCursor: Not supported on current platform.");
  }


  void D3D9Cursor::ResetHardwareCursor() {
    Logger::warn("D3D9Cursor::ResetHardwareCursor: Not supported on current platform.");
  }


  void D3D9Cursor::ResetSoftwareCursor() {
    Logger::warn("D3D9Cursor::ResetSoftwareCursor: Not supported on current platform.");
  }


  void D3D9Cursor::UpdateCursor(int X, int Y) {
    Logger::warn("D3D9Cursor::UpdateCursor: Not supported on current platform.");
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    Logger::warn("D3D9Cursor::ShowCursor: Not supported on current platform.");
    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    Logger::warn("D3D9Cursor::SetHardwareCursor: Not supported on current platform.");

    return D3D_OK;
  }

  HRESULT D3D9Cursor::SetSoftwareCursor(UINT Width, UINT Height, UINT XHotSpot, UINT YHotSpot) {
    Logger::warn("D3D9Cursor::SetSoftwareCursor: Not supported on current platform.");

    return D3D_OK;
  }
#endif

}
