#include <windows.h>

#include <SDL3/SDL.h>

namespace dxvk::wsi {

  inline SDL_Window* fromHwnd(HWND hWindow) {
    return reinterpret_cast<SDL_Window*>(hWindow);
  }

  inline HWND toHwnd(SDL_Window* pWindow) {
    return reinterpret_cast<HWND>(pWindow);
  }

  // Offset so null HMONITORs go to -1
  inline SDL_DisplayID fromHmonitor(HMONITOR hMonitor) {
    return SDL_DisplayID(reinterpret_cast<uintptr_t>(hMonitor));
  }

  // Offset so -1 display id goes to 0 == NULL
  inline HMONITOR toHmonitor(SDL_DisplayID display) {
    return reinterpret_cast<HMONITOR>(uintptr_t(display));
  }

}
