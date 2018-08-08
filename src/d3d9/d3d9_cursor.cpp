#include "d3d9_device.h"

namespace dxvk {
  // TODO: hardware cursor support.
  // We simply need to retain the state we are given here, and then render
  // the cursor's bitmap on top of the final image when Present()-ing.

  void D3D9Device::SetCursorPosition(int X, int Y, DWORD Flags) {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

  HRESULT D3D9Device::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  BOOL D3D9Device::ShowCursor(BOOL bShow) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
