#include "../wsi_monitor.h"

#include "wsi/native_wsi.h"
#include "wsi_platform_sdl2.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <string>
#include <sstream>


namespace dxvk::wsi {

  HMONITOR getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR enumMonitors(uint32_t index) {
    return isDisplayValid(int32_t(index))
      ? toHmonitor(index)
      : nullptr;
  }

  HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }

  bool getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    const int32_t displayId    = fromHmonitor(hMonitor);

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
    const int32_t displayId    = fromHmonitor(hMonitor);

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


  static inline uint32_t roundToNextPow2(uint32_t num) {
    if (num-- == 0)
      return 0;

    num |= num >> 1; num |= num >> 2;
    num |= num >> 4; num |= num >> 8;
    num |= num >> 16;

    return ++num;
  }


  static inline void convertMode(const SDL_DisplayMode& mode, WsiMode* pMode) {
    pMode->width          = uint32_t(mode.w);
    pMode->height         = uint32_t(mode.h);
    pMode->refreshRate    = WsiRational{ uint32_t(mode.refresh_rate) * 1000, 1000 }; 
    // BPP should always be a power of two
    // to match Windows behaviour of including padding.
    pMode->bitsPerPixel   = roundToNextPow2(SDL_BITSPERPIXEL(mode.format));
    pMode->interlaced     = false;
  }


  bool getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         ModeNumber,
          WsiMode*         pMode) {
    const int32_t displayId    = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    SDL_DisplayMode mode = { };
    if (SDL_GetDisplayMode(displayId, ModeNumber, &mode) != 0)
      return false;

    convertMode(mode, pMode);

    return true;
  }


  bool getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    const int32_t displayId    = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    SDL_DisplayMode mode = { };
    if (SDL_GetCurrentDisplayMode(displayId, &mode) != 0) {
      Logger::err(str::format("SDL_GetCurrentDisplayMode: ", SDL_GetError()));
      return false;
    }

    convertMode(mode, pMode);

    return true;
  }


  bool getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    const int32_t displayId    = fromHmonitor(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    SDL_DisplayMode mode = { };
    if (SDL_GetDesktopDisplayMode(displayId, &mode) != 0) {
      Logger::err(str::format("SDL_GetCurrentDisplayMode: ", SDL_GetError()));
      return false;
    }

    convertMode(mode, pMode);

    return true;
  }

  std::vector<uint8_t> getMonitorEdid(HMONITOR hMonitor) {
    Logger::err("getMonitorEdid not implemented on this platform.");
    return {};
  }

}
