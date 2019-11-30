#include <cstdint>
#include <windows.h>

#include <SDL2/SDL.h>

inline SDL_Window* window_cast(HWND hWindow) {
  return reinterpret_cast<SDL_Window*>(hWindow);
}

inline HWND window_cast(SDL_Window* pWindow) {
  return reinterpret_cast<HWND>(pWindow);
}

// Offset so null HMONITORs go to -1
inline int32_t monitor_cast(HMONITOR hMonitor) {
  return static_cast<int32_t>(reinterpret_cast<intptr_t>(hMonitor)) - 1;
}

// Offset so -1 display id goes to 0 == NULL
inline HMONITOR monitor_cast(int32_t displayId) {
  return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(displayId + 1));
}