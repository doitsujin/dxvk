#include <windows.h>

#include "native_external.h"

namespace dxvk::wsi {

  inline dxvk_external_window* fromHwnd(HWND hWindow) {
    return reinterpret_cast<dxvk_external_window*>(hWindow);
  }

  inline HWND toHwnd(dxvk_external_window* pWindow) {
    return reinterpret_cast<HWND>(pWindow);
  }

}
