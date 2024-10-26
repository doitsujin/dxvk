#if defined(DXVK_WSI_SDL2)

#include "../wsi_window.h"

#include "native/wsi/native_sdl2.h"
#include "wsi_platform_sdl2.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <windows.h>
#include <SDL_vulkan.h>

namespace dxvk::wsi {

  void Sdl2WsiDriver::getWindowSize(
        HWND      hWindow,
        uint32_t* pWidth,
        uint32_t* pHeight) {
    SDL_Window* window = fromHwnd(hWindow);

    int32_t w, h;
    SDL_GetWindowSize(window, &w, &h);

    if (pWidth)
      *pWidth = uint32_t(w);

    if (pHeight)
      *pHeight = uint32_t(h);
  }


  void Sdl2WsiDriver::resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         Width,
          uint32_t         Height) {
    SDL_Window* window = fromHwnd(hWindow);

    SDL_SetWindowSize(window, int32_t(Width), int32_t(Height));
  }


  bool Sdl2WsiDriver::setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
    const WsiMode&         pMode) {
    const int32_t displayId    = fromHmonitor(hMonitor);
    SDL_Window* window         = fromHwnd(hWindow);

    if (!isDisplayValid(displayId))
      return false;

    SDL_DisplayMode wantedMode = { };
    wantedMode.w            = pMode.width;
    wantedMode.h            = pMode.height;
    wantedMode.refresh_rate = pMode.refreshRate.numerator != 0
      ? pMode.refreshRate.numerator / pMode.refreshRate.denominator
      : 0;
    // TODO: Implement lookup format for bitsPerPixel here.

    SDL_DisplayMode mode = { };
    if (SDL_GetClosestDisplayMode(displayId, &wantedMode, &mode) == nullptr) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_GetClosestDisplayMode: ", SDL_GetError()));
      return false;
    }

    if (SDL_SetWindowDisplayMode(window, &mode) != 0) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_SetWindowDisplayMode: ", SDL_GetError()));
      return false;
    }

    return true;
  }



  bool Sdl2WsiDriver::enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             ModeSwitch) {
    const int32_t displayId    = fromHmonitor(hMonitor);
    SDL_Window* window         = fromHwnd(hWindow);

    if (!isDisplayValid(displayId))
      return false;

    uint32_t flags = ModeSwitch
        ? SDL_WINDOW_FULLSCREEN
        : SDL_WINDOW_FULLSCREEN_DESKTOP;
    
    // TODO: Set this on the correct monitor.
    // Docs aren't clear on this...
    if (SDL_SetWindowFullscreen(window, flags) != 0) {
      Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool Sdl2WsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    SDL_Window* window = fromHwnd(hWindow);

    if (SDL_SetWindowFullscreen(window, 0) != 0) {
      Logger::err(str::format("SDL2 WSI: leaveFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool Sdl2WsiDriver::restoreDisplayMode() {
    // Don't need to do anything with SDL2 here.
    return true;
  }


  HMONITOR Sdl2WsiDriver::getWindowMonitor(HWND hWindow) {
    SDL_Window* window      = fromHwnd(hWindow);
    const int32_t displayId = SDL_GetWindowDisplayIndex(window);

    return toHmonitor(displayId);
  }


  bool Sdl2WsiDriver::isWindow(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    return window != nullptr;
  }


  bool Sdl2WsiDriver::isMinimized(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
  }


  bool Sdl2WsiDriver::isOccluded(HWND hWindow) {
    return false;
  }


  void Sdl2WsiDriver::updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {
    // Don't need to do anything with SDL2 here.
  }


  VkResult Sdl2WsiDriver::createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    SDL_Window* window = fromHwnd(hWindow);

    return SDL_Vulkan_CreateSurface(window, instance, pSurface)
         ? VK_SUCCESS
         : VK_ERROR_OUT_OF_HOST_MEMORY;
  }

}

#endif
