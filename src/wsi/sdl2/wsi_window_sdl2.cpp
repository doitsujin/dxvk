#include "../wsi_window.h"

#include "native/wsi/native_wsi.h"
#include "wsi_platform_sdl2.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <windows.h>

namespace dxvk::wsi {

  void getWindowSize(
        HWND      hWindow,
        uint32_t* pWidth,
        uint32_t* pHeight) {
    SDL_Window* window = fromHwnd(hWindow);

    int32_t w, h;
    WsiLibrary::get()->SDL_GetWindowSize(window, &w, &h);

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
    SDL_Window* window = fromHwnd(hWindow);

    WsiLibrary::get()->SDL_SetWindowSize(window, int32_t(Width), int32_t(Height));
  }


  bool setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
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
    if (WsiLibrary::get()->SDL_GetClosestDisplayMode(displayId, &wantedMode, &mode) == nullptr) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_GetClosestDisplayMode: ", WsiLibrary::get()->SDL_GetError()));
      return false;
    }

    if (WsiLibrary::get()->SDL_SetWindowDisplayMode(window, &mode) != 0) {
      Logger::err(str::format("SDL2 WSI: setWindowMode: SDL_SetWindowDisplayMode: ", WsiLibrary::get()->SDL_GetError()));
      return false;
    }

    return true;
  }



  bool enterFullscreenMode(
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
    if (WsiLibrary::get()->SDL_SetWindowFullscreen(window, flags) != 0) {
      Logger::err(str::format("SDL2 WSI: enterFullscreenMode: SDL_SetWindowFullscreen: ", WsiLibrary::get()->SDL_GetError()));
      return false;
    }

    return true;
  }


  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    SDL_Window* window = fromHwnd(hWindow);

    if (WsiLibrary::get()->SDL_SetWindowFullscreen(window, 0) != 0) {
      Logger::err(str::format("SDL2 WSI: leaveFullscreenMode: SDL_SetWindowFullscreen: ", WsiLibrary::get()->SDL_GetError()));
      return false;
    }

    return true;
  }


  bool restoreDisplayMode() {
    // Don't need to do anything with SDL2 here.
    return true;
  }


  HMONITOR getWindowMonitor(HWND hWindow) {
    SDL_Window* window      = fromHwnd(hWindow);
    const int32_t displayId = WsiLibrary::get()->SDL_GetWindowDisplayIndex(window);

    return toHmonitor(displayId);
  }


  bool isWindow(HWND hWindow) {
    SDL_Window* window = fromHwnd(hWindow);
    return window != nullptr;
  }


  void updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {
    // Don't need to do anything with SDL2 here.
  }


  VkResult createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    SDL_Window* window = fromHwnd(hWindow);

    return WsiLibrary::get()->SDL_Vulkan_CreateSurface(window, instance, pSurface)
         ? VK_SUCCESS
         : VK_ERROR_OUT_OF_HOST_MEMORY;
  }


  std::vector<const char *> getInstanceExtensions() {
    WsiLibrary::get()->SDL_Vulkan_LoadLibrary(nullptr);

    uint32_t extensionCount = 0;
    if (!WsiLibrary::get()->SDL_Vulkan_GetInstanceExtensions(nullptr, &extensionCount, nullptr))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extension count. ", WsiLibrary::get()->SDL_GetError()));

    auto extensionNames = std::vector<const char *>(extensionCount);
    if (!WsiLibrary::get()->SDL_Vulkan_GetInstanceExtensions(nullptr, &extensionCount, extensionNames.data()))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extensions. ", WsiLibrary::get()->SDL_GetError()));

    return extensionNames;
  }

}
