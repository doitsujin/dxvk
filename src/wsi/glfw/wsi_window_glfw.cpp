#if defined(DXVK_WSI_GLFW)

#include "../wsi_window.h"

#include "native/wsi/native_glfw.h"
#include "wsi_platform_glfw.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <windows.h>
#include "../../vulkan/vulkan_loader.h"
#include <GLFW/glfw3.h>

#include <chrono>

namespace dxvk::wsi {

  void GlfwWsiDriver::getWindowSize(
      HWND hWindow,
      uint32_t* pWidth,
      uint32_t* pHeight) {
    GLFWwindow* window = fromHwnd(hWindow);

    int32_t w, h;
    glfwGetWindowSize(window, &w, &h);

    if (pWidth)
      *pWidth = uint32_t(w);

    if (pHeight)
      *pHeight = uint32_t(h);
  }


  void GlfwWsiDriver::resizeWindow(
      HWND hWindow,
      DxvkWindowState* pState,
      uint32_t Width,
      uint32_t Height) {
    GLFWwindow* window = fromHwnd(hWindow);

    glfwSetWindowSize(window, int32_t(Width), int32_t(Height));
  }


  void GlfwWsiDriver::saveWindowState(
      HWND Window,
      DxvkWindowState* pState,
      bool saveStyle) {
    if (!pState)
      return;

    GLFWwindow* window = fromHwnd(Window);
    auto& state = pState->glfw;

    glfwGetWindowPos(window, &state.x, &state.y);
    glfwGetWindowSize(window, &state.width, &state.height);
    state.decorated = glfwGetWindowAttrib(window, GLFW_DECORATED) == GLFW_TRUE;
    state.maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
    state.iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
    state.valid = true;

    (void)saveStyle;
  }


  void GlfwWsiDriver::restoreWindowState(
      HWND hWindow,
      DxvkWindowState* pState,
      bool restoreCoordinates) {
    if (!pState || !pState->glfw.valid)
      return;

    GLFWwindow* window = fromHwnd(hWindow);
    const auto& state = pState->glfw;

    if (restoreCoordinates)
      glfwSetWindowMonitor(window, nullptr, state.x, state.y, state.width, state.height, GLFW_DONT_CARE);

    glfwSetWindowAttrib(window, GLFW_DECORATED, state.decorated ? GLFW_TRUE : GLFW_FALSE);

    if (state.maximized)
      glfwMaximizeWindow(window);
    else if (state.iconified)
      glfwIconifyWindow(window);
    else
      glfwRestoreWindow(window);
  }


  bool GlfwWsiDriver::setWindowMode(
      HMONITOR hMonitor,
      HWND hWindow,
      DxvkWindowState* pState,
      const WsiMode& pMode) {
    const int32_t displayId = fromHmonitor(hMonitor);
    GLFWwindow* window = fromHwnd(hWindow);

    if (!isDisplayValid(displayId))
      return false;

    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);
    GLFWmonitor* monitor = monitors[displayId];

    GLFWvidmode wantedMode = {};
    wantedMode.width = pMode.width;
    wantedMode.height = pMode.height;
    wantedMode.refreshRate = pMode.refreshRate.numerator != 0
                 ? pMode.refreshRate.numerator / pMode.refreshRate.denominator
                 : 0;
    // TODO: Implement lookup format for bitsPerPixel here.

    glfwSetWindowMonitor(window, monitor, 0, 0, wantedMode.width, wantedMode.height, wantedMode.refreshRate);

    return true;
  }

  bool GlfwWsiDriver::enterFullscreenMode(
      HMONITOR hMonitor,
      HWND hWindow,
      DxvkWindowState* pState,
      bool ModeSwitch) {
    const int32_t displayId = fromHmonitor(hMonitor);
    GLFWwindow* window = fromHwnd(hWindow);

    if (!isDisplayValid(displayId))
      return false;

    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);
    GLFWmonitor* monitor = monitors[displayId];
    const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);

    int32_t mx = 0, my = 0;
    glfwGetMonitorPos(monitor, &mx, &my);

    if (ModeSwitch) {
      glfwSetWindowMonitor(window, monitor, mx, my,
        videoMode->width, videoMode->height, videoMode->refreshRate);
    } else {
      glfwSetWindowMonitor(window, nullptr, mx, my,
        videoMode->width, videoMode->height, GLFW_DONT_CARE);
    }

    return true;
  }


  bool GlfwWsiDriver::leaveFullscreenMode(
      HWND hWindow,
      DxvkWindowState* pState) {
    GLFWwindow* window = fromHwnd(hWindow);

    if (pState && pState->glfw.valid) {
      const auto& state = pState->glfw;
      glfwSetWindowMonitor(window, nullptr, state.x, state.y, state.width, state.height, GLFW_DONT_CARE);
    } else {
      glfwSetWindowMonitor(window, nullptr, 0, 0, 800, 600, GLFW_DONT_CARE);
    }

    return true;
  }


  bool GlfwWsiDriver::restoreDisplayMode() {
    // Don't need to do anything with GLFW here.
    return true;
  }


  HMONITOR GlfwWsiDriver::getWindowMonitor(HWND hWindow) {
    GLFWwindow* window = fromHwnd(hWindow);

    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    if (monitor) {
      int32_t displayCount = 0;
      GLFWmonitor** monitors = glfwGetMonitors(&displayCount);

      for (int32_t i = 0; i < displayCount; i++) {
        if (monitors[i] == monitor)
          return toHmonitor(i);
      }
    }

    int32_t wx = 0, wy = 0;
    glfwGetWindowPos(window, &wx, &wy);
    int32_t ww = 0, wh = 0;
    glfwGetWindowSize(window, &ww, &wh);

    const int32_t cx = wx + ww / 2;
    const int32_t cy = wy + wh / 2;

    int32_t displayCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&displayCount);

    for (int32_t i = 0; i < displayCount; i++) {
      int32_t mx = 0, my = 0;
      glfwGetMonitorPos(monitors[i], &mx, &my);
      const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);

      if (cx >= mx && cx < mx + mode->width
       && cy >= my && cy < my + mode->height)
        return toHmonitor(i);
    }

    return getDefaultMonitor();
  }


  bool GlfwWsiDriver::isWindow(HWND hWindow) {
    GLFWwindow* window = fromHwnd(hWindow);
    return window != nullptr;
  }


  bool GlfwWsiDriver::isMinimized(HWND hWindow) {
    GLFWwindow* window = fromHwnd(hWindow);
    return glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
  }


  bool GlfwWsiDriver::isOccluded(HWND hWindow) {
    GLFWwindow* window = fromHwnd(hWindow);

    const bool hasFocus = glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;

    // Use milliseconds via steady_clock for a platform-neutral timestamp.
    const uint64_t nowMs = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());

    if (hasFocus) {
      m_lastFocusTimestamp = nowMs;
      return false;
    }

    return m_lastFocusTimestamp != 0 && nowMs - m_lastFocusTimestamp > 100;
  }


  void GlfwWsiDriver::updateFullscreenWindow(
      HMONITOR hMonitor,
      HWND     hWindow,
      bool     forceTopmost) {
    // Don't need to do anything with GLFW here.
  }

  VkResult GlfwWsiDriver::createSurface(
      HWND hWindow,
      PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
      VkInstance                instance,
      VkSurfaceKHR* pSurface) {
    GLFWwindow* window = fromHwnd(hWindow);

    return glfwCreateWindowSurface(instance, window, nullptr, pSurface);
  }

}

#endif
