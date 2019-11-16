#include "../wsi_window.h"

#include "wsi_helpers_sdl2.h"

#include <windows.h>

#include "../../util/util_string.h"
#include "../../util/log/log.h"

namespace dxvk::wsi {

  void getWindowSize(
        HWND      hWindow,
        uint32_t* pWidth,
        uint32_t* pHeight) {
    int32_t w, h;
    SDL_GetWindowSize(hWindow, &w, &h);

    if (pWidth)
      *pWidth = uint32_t(w);

    if (pHeight)
      *pHeight = uint32_t(h);
  }


  void resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         Width,
          uint32_t         Height) {
    SDL_SetWindowSize(hWindow, int32_t(Width), int32_t(Height));
  }


  bool setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
    const WsiMode*         pMode,
          bool             EnteringFullscreen) {
    const int32_t displayId    = monitor_cast(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    SDL_DisplayMode wantedMode = { };
    wantedMode.w            = pMode->width;
    wantedMode.h            = pMode->height;
    wantedMode.refresh_rate = pMode->refreshRate.numerator != 0
      ? pMode->refreshRate.numerator / pMode->refreshRate.denominator
      : 0;
    // TODO: Implement lookup format for bitsPerPixel here.

    SDL_DisplayMode mode = { };
    if (SDL_GetClosestDisplayMode(displayId, &wantedMode, &mode) == nullptr) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_GetClosestDisplayMode: ", SDL_GetError()));
      return false;
    }

    if (SDL_SetWindowDisplayMode(hWindow, &mode) != 0) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_SetWindowDisplayMode: ", SDL_GetError()));
      return false;
    }

    return true;
  }



  bool enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             ModeSwitch) {
    const int32_t displayId    = monitor_cast(hMonitor);

    if (!isDisplayValid(displayId))
      return false;

    uint32_t flags = ModeSwitch
        ? SDL_WINDOW_FULLSCREEN
        : SDL_WINDOW_FULLSCREEN_DESKTOP;
    
    // TODO: Set this on the correct monitor.
    // Docs aren't clear on this...
    if (SDL_SetWindowFullscreen(hWindow, flags) != 0) {
      Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
    if (SDL_SetWindowFullscreen(hWindow, 0) != 0) {
      Logger::err(str::format("SDL2 WSI: leaveFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool restoreDisplayMode(HMONITOR hMonitor) {
    return true;
  }


  HMONITOR getWindowMonitor(HWND hWindow) {
    const int32_t displayId = SDL_GetWindowDisplayIndex(hWindow);

    return monitor_cast(displayId);
  }

}