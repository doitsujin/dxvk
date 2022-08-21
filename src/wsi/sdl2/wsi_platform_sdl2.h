#pragma once

#include <SDL2/SDL.h>

#include "../wsi_monitor.h"

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
  };

  inline bool isDisplayValid(int32_t displayId) {
    const int32_t displayCount = SDL_GetNumVideoDisplays();

    return displayId < displayCount && displayId >= 0;
  }
  
}