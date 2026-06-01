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


  void Sdl2WsiDriver::saveWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             saveStyle) {
    if (!pState)
      return;

    SDL_Window* window = fromHwnd(hWindow);
    auto& state = pState->sdl2;

    SDL_GetWindowPosition(window, &state.x, &state.y);
    SDL_GetWindowSize(window, &state.width, &state.height);
    state.windowFlags = SDL_GetWindowFlags(window);
    state.valid = true;

    (void)saveStyle;
  }


  void Sdl2WsiDriver::restoreWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    if (!pState || !pState->sdl2.valid)
      return;

    SDL_Window* window = fromHwnd(hWindow);
    const auto& state = pState->sdl2;

    if (restoreCoordinates) {
      SDL_SetWindowPosition(window, state.x, state.y);
      SDL_SetWindowSize(window, state.width, state.height);
    }

    SDL_SetWindowBordered(window,
      (state.windowFlags & SDL_WINDOW_BORDERLESS) ? SDL_FALSE : SDL_TRUE);

    if (state.windowFlags & SDL_WINDOW_MAXIMIZED)
      SDL_MaximizeWindow(window);
    else if (state.windowFlags & SDL_WINDOW_MINIMIZED)
      SDL_MinimizeWindow(window);
    else if (!(state.windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)))
      SDL_RestoreWindow(window);
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

    // Save the requested mode so enterFullscreenMode can use it instead of
    // falling back to the desktop display mode (parity with SDL3 backend).
    if (pState)
      pState->sdl2.fullscreenMode = pMode;

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

    if (ModeSwitch) {
      // Use the mode previously saved by setWindowMode when available.
      // This matches the SDL3 backend which stores the requested mode in
      // pState->sdl3.fullscreenMode and then calls SDL_GetClosestFullscreenDisplayMode
      // in enterFullscreenMode, rather than always fetching the desktop mode.
      SDL_DisplayMode wantedMode = { };
      if (pState && pState->sdl2.fullscreenMode.width != 0) {
        const auto& saved = pState->sdl2.fullscreenMode;
        wantedMode.w            = saved.width;
        wantedMode.h            = saved.height;
        wantedMode.refresh_rate = saved.refreshRate.numerator != 0
          ? saved.refreshRate.numerator / saved.refreshRate.denominator
          : 0;
      }

      SDL_DisplayMode closestMode = { };
      if (wantedMode.w != 0) {
        if (SDL_GetClosestDisplayMode(displayId, &wantedMode, &closestMode) == nullptr) {
          Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_GetClosestDisplayMode: ", SDL_GetError()));
          return false;
        }
      } else {
        if (SDL_GetDesktopDisplayMode(displayId, &closestMode) != 0) {
          Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_GetDesktopDisplayMode: ", SDL_GetError()));
          return false;
        }
      }

      if (SDL_SetWindowDisplayMode(window, &closestMode) != 0) {
        Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowDisplayMode: ", SDL_GetError()));
        return false;
      }
    }

    // Position the window on the target display before going fullscreen.
    if (SDL_SetWindowPosition(window,
          SDL_WINDOWPOS_CENTERED_DISPLAY(displayId),
          SDL_WINDOWPOS_CENTERED_DISPLAY(displayId)) != 0) {
      Logger::warn(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowPosition: ", SDL_GetError()));
    }

    uint32_t flags = ModeSwitch
        ? SDL_WINDOW_FULLSCREEN
        : SDL_WINDOW_FULLSCREEN_DESKTOP;

    if (SDL_SetWindowFullscreen(window, flags) != 0) {
      Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", SDL_GetError()));
      return false;
    }

    return true;
  }


  bool Sdl2WsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
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
    SDL_Window* window = fromHwnd(hWindow);

    const bool hasFocus = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;

    if (hasFocus) {
      m_lastFocusTimestamp = SDL_GetTicks();
      return false;
    }

    return m_lastFocusTimestamp != 0 && SDL_GetTicks() - m_lastFocusTimestamp > 100;
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
