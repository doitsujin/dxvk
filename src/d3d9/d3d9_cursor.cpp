#include "d3d9_cursor.h"

namespace dxvk {
  void D3D9DeviceCursor::SetCursorPosition(int X, int Y, DWORD Flags) {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

  HRESULT D3D9DeviceCursor::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  BOOL D3D9DeviceCursor::ShowCursor(BOOL bShow) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
