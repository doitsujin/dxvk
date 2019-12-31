#include "d3d9_cursor.h"

#include <utility>

namespace dxvk {

#ifndef DXVK_NATIVE
  void D3D9Cursor::UpdateCursor(int X, int Y) {
    ::SetCursorPos(X, Y);
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    ::SetCursor(bShow ? m_hCursor : nullptr);
    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    DWORD mask[32];
    std::memset(mask, ~0, sizeof(mask));

    ICONINFO info;
    info.fIcon    = FALSE;
    info.xHotspot = XHotSpot;
    info.yHotspot = YHotSpot;
    info.hbmMask  = ::CreateBitmap(HardwareCursorWidth, HardwareCursorHeight, 1, 1,  mask);
    info.hbmColor = ::CreateBitmap(HardwareCursorWidth, HardwareCursorHeight, 1, 32, &bitmap[0]);

    if (m_hCursor != nullptr)
      ::DestroyCursor(m_hCursor);

    m_hCursor = ::CreateIconIndirect(&info);

    ::DeleteObject(info.hbmMask);
    ::DeleteObject(info.hbmColor);

    ShowCursor(m_visible);

    return D3D_OK;
  }
#else
  void D3D9Cursor::UpdateCursor(int X, int Y) { 
    Logger::warn("D3D9Cursor::UpdateCursor: Not supported on native");
  }


  BOOL D3D9Cursor::ShowCursor(BOOL bShow) {
    Logger::warn("D3D9Cursor::ShowCursor: Not supported on native");

    return std::exchange(m_visible, bShow);
  }


  HRESULT D3D9Cursor::SetHardwareCursor(UINT XHotSpot, UINT YHotSpot, const CursorBitmap& bitmap) {
    Logger::warn("D3D9Cursor::SetHardwareCursor: Not supported on native");

    return D3DERR_INVALIDCALL;
  }
#endif

}