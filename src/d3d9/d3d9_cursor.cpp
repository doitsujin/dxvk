#include "d3d9_cursor.h"
#include "d3d9_util.h"

#ifndef _WIN32
#include "d3d9_cursor_native.h"
#endif

#include <utility>

namespace dxvk {

#ifdef _WIN32

  void D3D9Cursor::ResetCursor() {
    m_visible = FALSE;
    ShowCursor(m_visible);

    if (IsHardwareCursor())
      ResetHardwareCursor();
    else if (IsActiveSoftwareCursor())
      ResetSoftwareCursor();
  }


  void D3D9Cursor::ResetHardwareCursor() {
    ::DestroyCursor(m_hCursor);
    m_hCursor = nullptr;
  }


  void D3D9Cursor::ResetSoftwareCursor() {
    m_sCursor.DrawCursor  = false;
    m_sCursor.ClearCursor = true;
  }


  void D3D9Cursor::UpdateCursor(int X, int Y) {
    // SetCursorPosition is used to directly update the position of software cursors,
    // but keep track of the cursor position even when using hardware cursors, in order
    // to ensure a smooth transition/overlap from one type to the other.
    m_sCursor.X = X;
    m_sCursor.Y = Y;

    if (IsActiveSoftwareCursor())
      return;

    POINT currentPos = { };
    if (::GetCursorPos(&currentPos) && currentPos == POINT{ X, Y })
        return;

    ::SetCursorPos(X, Y);
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    // Cursor visibility remains unchanged (typically FALSE) if the cursor isn't set.
    if (unlikely(!IsHardwareCursor() && !IsActiveSoftwareCursor()))
      return m_visible;

    if (IsHardwareCursor())
      ::SetCursor(bShow ? m_hCursor : nullptr);
    else
      m_sCursor.DrawCursor = bShow;

    return std::exchange(m_visible, bShow);
  }


  void D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    if (IsActiveSoftwareCursor())
      ResetSoftwareCursor();

    CursorMask mask;
    std::memset(mask, ~0, sizeof(mask));

    ICONINFO info;
    info.fIcon    = FALSE;
    info.xHotspot = XHotSpot;
    info.yHotspot = YHotSpot;
    info.hbmMask  = ::CreateBitmap(HardwareCursorWidth, HardwareCursorHeight, 1, 1,  &mask[0]);
    info.hbmColor = ::CreateBitmap(HardwareCursorWidth, HardwareCursorHeight, 1, 32, &bitmap[0]);

    if (IsHardwareCursor())
      ::DestroyCursor(m_hCursor);

    m_hCursor = ::CreateIconIndirect(&info);

    ::DeleteObject(info.hbmMask);
    ::DeleteObject(info.hbmColor);

    ShowCursor(m_visible);
  }


  void D3D9Cursor::SetSoftwareCursor(UINT XHotSpot, UINT YHotSpot, UINT Width, UINT Height) {
    // Make sure to hide the win32 cursor.
    ::SetCursor(nullptr);

    if (IsHardwareCursor())
      ResetHardwareCursor();

    m_sCursor.Width       = Width;
    m_sCursor.Height      = Height;
    m_sCursor.XHotSpot    = XHotSpot;
    m_sCursor.YHotSpot    = YHotSpot;
    m_sCursor.ClearCursor = false;

    ShowCursor(m_visible);
  }

#else

  D3D9Cursor::~D3D9Cursor() {
    native_cursor::Reset();
  }


  void D3D9Cursor::SetWindow(HWND window) {
    m_window = window;
    native_cursor::SetWindow(window);
  }


  bool D3D9Cursor::IsHardwareCursor() const {
    return native_cursor::HasHardwareCursor();
  }


  void D3D9Cursor::ResetCursor() {
    m_visible = FALSE;
    ShowCursor(m_visible);

    if (IsHardwareCursor())
      ResetHardwareCursor();
    else if (IsActiveSoftwareCursor())
      ResetSoftwareCursor();
  }


  void D3D9Cursor::ResetHardwareCursor() {
    native_cursor::ResetHardwareCursor();
  }


  void D3D9Cursor::ResetSoftwareCursor() {
    m_sCursor.DrawCursor  = false;
    m_sCursor.ClearCursor = true;
  }


  void D3D9Cursor::UpdateCursor(int X, int Y) {
    m_sCursor.X = X;
    m_sCursor.Y = Y;

    if (IsActiveSoftwareCursor())
      return;

    if (IsHardwareCursor() && m_window != nullptr)
      native_cursor::UpdateHardwareCursorPosition(m_window, X, Y);
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    if (unlikely(!IsHardwareCursor() && !IsActiveSoftwareCursor()))
      return m_visible;

    if (IsHardwareCursor()) {
      if (m_window != nullptr)
        native_cursor::ShowHardwareCursor(m_window, bShow);
    } else {
      m_sCursor.DrawCursor = bShow;
    }

    return std::exchange(m_visible, bShow);
  }


  void D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    if (IsActiveSoftwareCursor())
      ResetSoftwareCursor();

    if (m_window != nullptr)
      native_cursor::SetHardwareCursor(m_window, XHotSpot, YHotSpot, bitmap);

    ShowCursor(m_visible);
  }


  void D3D9Cursor::SetSoftwareCursor(UINT XHotSpot, UINT YHotSpot, UINT Width, UINT Height) {
    if (m_window != nullptr)
      native_cursor::PrepareSoftwareCursor(m_window);

    if (IsHardwareCursor())
      ResetHardwareCursor();

    m_sCursor.Width       = Width;
    m_sCursor.Height      = Height;
    m_sCursor.XHotSpot    = XHotSpot;
    m_sCursor.YHotSpot    = YHotSpot;
    m_sCursor.ClearCursor = false;

    ShowCursor(m_visible);
  }

#endif

}
