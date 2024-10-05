#if defined(DXVK_WSI_SDL3)

#include "../wsi_monitor.h"

#include "wsi/native_sdl3.h"
#include "wsi_platform_sdl3.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <string>
#include <sstream>


namespace dxvk::wsi {

  HMONITOR Sdl3WsiDriver::getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR Sdl3WsiDriver::enumMonitors(uint32_t index) {
    int count = 0;

    SDL_DisplayID* displays = SDL_GetDisplays(&count);

    HMONITOR result = displays && int(index) < count
      ? toHmonitor(displays[index])
      : nullptr;

    SDL_free(displays);
    return result;
  }


  HMONITOR Sdl3WsiDriver::enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }


  bool Sdl3WsiDriver::getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!displayId)
      return false;

    std::wstringstream nameStream;
    nameStream << LR"(\\.\DISPLAY)" << displayId;

    std::wstring name = nameStream.str();

    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);

    return true;
  }


  bool Sdl3WsiDriver::getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!displayId)
      return false;

    SDL_Rect rect = { };
    SDL_GetDisplayBounds(displayId, &rect);

    pRect->left   = rect.x;
    pRect->top    = rect.y;
    pRect->right  = rect.x + rect.w;
    pRect->bottom = rect.y + rect.h;

    return true;
  }


  bool Sdl3WsiDriver::getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         ModeNumber,
          WsiMode*         pMode) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!displayId)
      return false;

    int count = 0;
    auto* modes = SDL_GetFullscreenDisplayModes(displayId, &count);

    if (!modes) {
      Logger::err(str::format("SDL_GetFullscreenDisplayModes: ", SDL_GetError()));
      return false;
    }

    if (int(ModeNumber) >= count) {
      SDL_free(modes);
      return false;
    }

    convertMode(*modes[ModeNumber], pMode);

    SDL_free(modes);
    return true;
  }


  bool Sdl3WsiDriver::getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!displayId)
      return false;

    auto* mode = SDL_GetCurrentDisplayMode(displayId);

    if (!mode) {
      Logger::err(str::format("SDL_GetCurrentDisplayMode: ", SDL_GetError()));
      return false;
    }

    convertMode(*mode, pMode);
    return true;
  }


  bool Sdl3WsiDriver::getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!displayId)
      return false;

    auto* mode = SDL_GetDesktopDisplayMode(displayId);

    if (!mode) {
      Logger::err(str::format("SDL_GetDesktopDisplayMode: ", SDL_GetError()));
      return false;
    }

    convertMode(*mode, pMode);
    return true;
  }


  std::vector<uint8_t> Sdl3WsiDriver::getMonitorEdid(HMONITOR hMonitor) {
    Logger::err("getMonitorEdid not implemented on this platform.");
    return {};
  }

}

#endif
