#include "../wsi_monitor.h"

#include "wsi_helpers_sdl2.h"

#include <windows.h>
#include <wsi/native_wsi.h>

#include <string>
#include <sstream>

namespace dxvk::wsi {

  HMONITOR enumMonitors(uint32_t index) {
    return isDisplayValid(int32_t(index))
      ? monitor_cast(index)
      : nullptr;
  }


  bool getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    const int32_t displayId    = monitor_cast(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    std::wstringstream nameStream;
    nameStream << LR"(\\.\DISPLAY)" << (displayId + 1);

    std::wstring name = nameStream.str();

    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);

    return true;
  }


  bool getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    const int32_t displayId    = monitor_cast(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    SDL_Rect rect = { };
    SDL_GetDisplayBounds(displayId, &rect);

    pRect->left   = rect.x;
    pRect->top    = rect.y;
    pRect->right  = rect.x + rect.w;
    pRect->bottom = rect.y + rect.h;

    return true;
  }

}