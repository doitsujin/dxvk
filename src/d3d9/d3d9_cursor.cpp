#include "d3d9_cursor.h"
#include "d3d9_util.h"

#include <utility>

namespace dxvk {

#ifdef _WIN32
  void D3D9Cursor::ResetCursor() {
    ShowCursor(FALSE);

    if (likely(m_hCursor != nullptr)) {
      ::DestroyCursor(m_hCursor);
      m_hCursor = nullptr;
    } else {
      m_sCursor.Width = 0;
      m_sCursor.Height = 0;
      m_sCursor.X = 0;
      m_sCursor.Y = 0;
    }
  }


  void D3D9Cursor::UpdateCursor(int X, int Y) {
    POINT currentPos = { };
    if (::GetCursorPos(&currentPos) && currentPos == POINT{ X, Y })
        return;

    ::SetCursorPos(X, Y);
  }


  void D3D9Cursor::RefreshSoftwareCursorPosition() {
    POINT currentPos = { };
    ::GetCursorPos(&currentPos);

    m_sCursor.X = static_cast<UINT>(currentPos.x);
    m_sCursor.Y = static_cast<UINT>(currentPos.y);
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    if (likely(m_hCursor != nullptr))
      ::SetCursor(bShow ? m_hCursor : nullptr);
    
    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    if (unlikely(IsSoftwareCursor())) {
      m_sCursor.Width = 0;
      m_sCursor.Height = 0;
      m_sCursor.X = 0;
      m_sCursor.Y = 0;
    }

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

    if (unlikely(m_hCursor != nullptr)) {
      ::DestroyCursor(m_hCursor);
      m_hCursor = nullptr;
    }

    m_sCursor.Width  = Width;
    m_sCursor.Height = Height;
    m_sCursor.X      = XHotSpot;
    m_sCursor.Y      = YHotSpot;

    ShowCursor(m_visible);

    return D3D_OK;
  }

#else
  void D3D9Cursor::ResetCursor() {
    Logger::warn("D3D9Cursor::ResetCursor: Not supported on current platform.");
  }


  void D3D9Cursor::UpdateCursor(int X, int Y) {
    Logger::warn("D3D9Cursor::UpdateCursor: Not supported on current platform.");
  }


  void D3D9Cursor::RefreshSoftwareCursorPosition() {
    Logger::warn("D3D9Cursor::RefreshSoftwareCursorPosition: Not supported on current platform.");
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
