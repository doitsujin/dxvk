#include "windows_base.h"

using HWND = SDL_Window*;

// Offset so null HMONITORs go to -1
inline int32_t monitor_cast(HMONITOR hMonitor) {
  return static_cast<int32_t>(reinterpret_cast<intptr_t>(hMonitor)) - 1;
}

// Offset so -1 display id goes to 0 == NULL
inline HMONITOR monitor_cast(int32_t displayId) {
  return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(displayId + 1));
}

inline BOOL IsWindow(HWND hWnd) { return hWnd != nullptr; }