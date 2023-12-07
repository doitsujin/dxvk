#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "../wsi_monitor.h"

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
  };

  #define SDL_PROC(ret, name, params) \
    typedef ret (SDLCALL *pfn_##name) params;
  #include "wsi_platform_sdl2_funcs.h"

  struct WsiLibrary {
    WsiLibrary(HMODULE dll);
    static WsiLibrary *get();
    HMODULE libsdl;

    #define SDL_PROC(ret, name, params) \
      pfn_##name name;
    #include "wsi_platform_sdl2_funcs.h"
  };

  void init();
  void quit();

  inline bool isDisplayValid(int32_t displayId) {
    const int32_t displayCount = WsiLibrary::get()->SDL_GetNumVideoDisplays();

    return displayId < displayCount && displayId >= 0;
  }
}
