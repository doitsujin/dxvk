#include "wsi_platform.h"
#include "wsi_monitor.h"
#include "wsi_window.h"
#include "../util/util_env.h"
#include "../util/util_error.h"

namespace dxvk::wsi {
  static WsiDriver* s_driver = nullptr;
  static int s_refcount = 0;

  static const WsiBootstrap *wsiBootstrap[] = {
#if defined(DXVK_WSI_WIN32)
    &Win32WSI,
#endif
#if defined(DXVK_WSI_SDL3)
    &Sdl3WSI,
#endif
#if defined(DXVK_WSI_SDL2)
    &Sdl2WSI,
#endif
#if defined(DXVK_WSI_GLFW)
    &GlfwWSI,
#endif
  };

  void init() {
    if (s_refcount++ > 0)
      return;

    std::string hint = dxvk::env::getEnvVar("DXVK_WSI_DRIVER");
    if (hint == "") {
        // At least for Windows, it is reasonable to fall back to a default;
        // for other platforms however we _need_ to know which WSI to use!
#if defined(DXVK_WSI_WIN32)
        hint = "Win32";
#else
        throw DxvkError("DXVK_WSI_DRIVER environment variable unset");
#endif
    }

    bool success = false;
    for (const WsiBootstrap *b : wsiBootstrap) {
      if (hint == b->name && b->createDriver(&s_driver)) {
        success = true;
        break;
      }
    }

    if (!success)
      throw DxvkError("Failed to initialize WSI.");
  }

  void quit() {
    if (s_refcount == 0)
      return;

    s_refcount--;
    if (s_refcount == 0) {
      delete s_driver;
      s_driver = nullptr;
    }
  }

  std::vector<const char *> getInstanceExtensions() {
    return s_driver->getInstanceExtensions();
  }

  void getWindowSize(
          HWND      hWindow,
          uint32_t* pWidth,
          uint32_t* pHeight) {
    s_driver->getWindowSize(hWindow, pWidth, pHeight);
  }

  void resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         width,
          uint32_t         height) {
    s_driver->resizeWindow(hWindow, pState, width, height);
  }

  void saveWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             saveStyle) {
    s_driver->saveWindowState(hWindow, pState, saveStyle);
  }

  void restoreWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    s_driver->restoreWindowState(hWindow, pState, restoreCoordinates);
  }

  bool setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
    const WsiMode&         mode) {
    return s_driver->setWindowMode(hMonitor, hWindow, pState, mode);
  }

  bool enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          [[maybe_unused]]
          bool             modeSwitch) {
    return s_driver->enterFullscreenMode(hMonitor, hWindow, pState, modeSwitch);
  }

  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
    return s_driver->leaveFullscreenMode(hWindow, pState);
  }

  bool restoreDisplayMode() {
    return s_driver->restoreDisplayMode();
  }

  HMONITOR getWindowMonitor(HWND hWindow) {
    return s_driver->getWindowMonitor(hWindow);
  }

  bool isWindow(HWND hWindow) {
    return s_driver->isWindow(hWindow);
  }

  bool isMinimized(HWND hWindow) {
    return s_driver->isMinimized(hWindow);
  }

  bool isOccluded(HWND hWindow) {
    return s_driver->isOccluded(hWindow);
  }

  void updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {
    s_driver->updateFullscreenWindow(hMonitor, hWindow, forceTopmost);
  }

  VkResult createSurface(
          HWND                hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance          instance,
          VkSurfaceKHR*       pSurface) {
    return s_driver->createSurface(hWindow, pfnVkGetInstanceProcAddr, instance, pSurface);
  }

  HMONITOR getDefaultMonitor() {
    return s_driver->getDefaultMonitor();
  }

  HMONITOR enumMonitors(uint32_t index) {
    return s_driver->enumMonitors(index);
  }

  HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return s_driver->enumMonitors(adapterLUID, numLUIDs, index);
  }

  bool getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    return s_driver->getDisplayName(hMonitor, Name);
  }

  bool getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    return s_driver->getDesktopCoordinates(hMonitor, pRect);
  }

  bool getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         modeNumber,
          WsiMode*         pMode) {
    return s_driver->getDisplayMode(hMonitor, modeNumber, pMode);
  }

  bool getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return s_driver->getCurrentDisplayMode(hMonitor, pMode);
  }

  bool getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return s_driver->getDesktopDisplayMode(hMonitor, pMode);
  }

  WsiEdidData getMonitorEdid(HMONITOR hMonitor) {
    return s_driver->getMonitorEdid(hMonitor);
  }

}
