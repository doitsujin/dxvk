#pragma once

#include "d3d9_cursor.h"

#include <windows.h>

namespace dxvk::native_cursor {

  void SetWindow(HWND window);

  void Reset();

  bool HasHardwareCursor();

  void SetHardwareCursor(
          HWND               window,
          UINT               hotX,
          UINT               hotY,
    const CursorBitmap&      bitmap);

  void ResetHardwareCursor();

  void PrepareSoftwareCursor(HWND window);

  void ShowHardwareCursor(HWND window, BOOL visible);

  void UpdateHardwareCursorPosition(HWND window, int x, int y);

}
