#if defined(DXVK_WSI_SDL3)

#include "../wsi_window.h"

#include "native/wsi/native_sdl3.h"
#include "wsi_platform_sdl3.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <windows.h>
#include <SDL3/SDL_vulkan.h>

namespace dxvk::wsi {

  void Sdl3WsiDriver::getWindowSize(
        HWND      hWindow,
        uint32_t* pWidth,
        uint32_t* pHeight) {
    SDL_Window* window = fromHwnd(hWindow);

    int w = 0;
    int h = 0;

    if (!SDL_GetWindowSizeInPixels(window, &w, &h))
      Logger::err(str::format("SDL3 WSI: SDL_GetWindowSizeinPixels: ", SDL_GetError()));

    if (pWidth)
      *pWidth = uint32_t(w);

    if (pHeight)
      *pHeight = uint32_t(h);
  }


  void Sdl3WsiDriver::resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         Width,
          uint32_t         Height) {
    SDL_Window* window = fromHwnd(hWindow);

    if (!SDL_SetWindowSize(window, int32_t(Width), int32_t(Height)))
      Logger::err(str::format("SDL3 WSI: SDL_SetWindowSize: ", SDL_GetError()));
  }


  bool Sdl3WsiDriver::setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
    const WsiMode&         pMode) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);

    if (!displayId)
      return false;

    pState->sdl3.fullscreenMode = pMode;
    return true;
  }



  bool Sdl3WsiDriver::enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             ModeSwitch) {
    SDL_DisplayID displayId = fromHmonitor(hMonitor);
    SDL_Window* window = fromHwnd(hWindow);

    if (!displayId)
      return false;

    SDL_Rect bounds = { };

    if (!SDL_GetDisplayUsableBounds(displayId, &bounds)) {
      Logger::err(str::format("SDL3 WSI: enterFullscreenMode: SDL_GetDisplayUsableBounds: ", SDL_GetError()));
      return false;
    }

    if (!SDL_SetWindowPosition(window, bounds.x, bounds.y)) {
      Logger::err(str::format("SDL3 WSI: enterFullscreenMode: SDL_SetWindowPosition: ", SDL_GetError()));
      return false;
    }

    SDL_DisplayMode closestMode = { };

    if (ModeSwitch) {
      const auto& mode = pState->sdl3.fullscreenMode;

      if (!SDL_GetClosestFullscreenDisplayMode(displayId, mode.width, mode.height,
          float(mode.refreshRate.numerator) / float(mode.refreshRate.denominator), true, &closestMode)) {
        Logger::err(str::format("SDL3 WSI: enterFullscreenMode: SDL_GetClosestFullscreenDisplayMode: ", SDL_GetError()));
        return false;
      }
    }

    if (!SDL_SetWindowFullscreenMode(window, ModeSwitch ? &closestMode : nullptr)) {
      Logger::err(str::format("SDL3 WSI: enterFullscreenMode: SDL_SetWindowFullscreenMode: ", SDL_GetError()));
      return false;
    }

    if (!SDL_SetWindowFullscreen(window, true)) {
      Logger::err(str::format("SDL3 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool Sdl3WsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    SDL_Window* window = fromHwnd(hWindow);

    if (!SDL_SetWindowFullscreen(window, false)) {
      Logger::err(str::format("SDL3 WSI: leaveFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool Sdl3WsiDriver::restoreDisplayMode() {
    // Don't need to do anything with SDL3 here.
    return true;
  }


  HMONITOR Sdl3WsiDriver::getWindowMonitor(HWND hWindow) {
    return toHmonitor(SDL_GetDisplayForWindow(fromHwnd(hWindow)));
  }


  bool Sdl3WsiDriver::isWindow(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    return window != nullptr;
  }


  bool Sdl3WsiDriver::isMinimized(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
  }


  bool Sdl3WsiDriver::isOccluded(HWND hWindow) {
    return false;
  }


  void Sdl3WsiDriver::updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {
    // Don't need to do anything with SDL3 here.
  }


  VkResult Sdl3WsiDriver::createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    SDL_Window* window = fromHwnd(hWindow);

    return SDL_Vulkan_CreateSurface(window, instance, nullptr, pSurface)
      ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
  }

}

#endif
