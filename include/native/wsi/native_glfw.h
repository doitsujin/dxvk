#include <windows.h>

#include <GLFW/glfw3.h>

namespace dxvk::wsi {

  inline GLFWwindow* fromHwnd(HWND hWindow) {
    return reinterpret_cast<GLFWwindow*>(hWindow);
  }

  inline HWND toHwnd(GLFWwindow* pWindow) {
    return reinterpret_cast<HWND>(pWindow);
  }

  // Offset so null HMONITORs go to -1
  inline int32_t fromHmonitor(HMONITOR hMonitor) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(hMonitor)) - 1;
  }

  // Offset so -1 display id goes to 0 == NULL
  inline HMONITOR toHmonitor(int32_t displayId) {
    return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(displayId + 1));
  }

}