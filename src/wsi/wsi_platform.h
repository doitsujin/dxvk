#pragma once

#include "wsi_window.h"

#include <vector>

namespace dxvk::wsi {

  class WsiDriver {
  public:
    virtual ~WsiDriver() {
    }

    // Platform
    virtual std::vector<const char *> getInstanceExtensions() = 0;

    // Monitor
    virtual HMONITOR getDefaultMonitor() = 0;

    virtual HMONITOR enumMonitors(uint32_t index) = 0;

    virtual HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) = 0;

    virtual bool getDisplayName(
            HMONITOR         hMonitor,
            WCHAR            (&Name)[32]) = 0;

    virtual bool getDesktopCoordinates(
            HMONITOR         hMonitor,
            RECT*            pRect) = 0;

    virtual bool getDisplayMode(
            HMONITOR         hMonitor,
            uint32_t         modeNumber,
            WsiMode*         pMode) = 0;

    virtual bool getCurrentDisplayMode(
            HMONITOR         hMonitor,
            WsiMode*         pMode) = 0;

    virtual bool getDesktopDisplayMode(
            HMONITOR         hMonitor,
            WsiMode*         pMode) = 0;

    virtual WsiEdidData getMonitorEdid(HMONITOR hMonitor) = 0;

    // Window

    virtual void getWindowSize(
            HWND      hWindow,
            uint32_t* pWidth,
            uint32_t* pWeight) = 0;

    virtual void resizeWindow(
            HWND             hWindow,
            DxvkWindowState* pState,
            uint32_t         width,
            uint32_t         weight) = 0;

    virtual void saveWindowState(
            HWND             hWindow,
            DxvkWindowState* pState,
            bool             saveStyle) = 0;

    virtual void restoreWindowState(
            HWND             hWindow,
            DxvkWindowState* pState,
            bool             restoreCoordinates) = 0;

    virtual bool setWindowMode(
            HMONITOR         hMonitor,
            HWND             hWindow,
            DxvkWindowState* pState,
      const WsiMode&         mode) = 0;

    virtual bool enterFullscreenMode(
            HMONITOR         hMonitor,
            HWND             hWindow,
            DxvkWindowState* pState,
            [[maybe_unused]]
            bool             modeSwitch) = 0;

    virtual bool leaveFullscreenMode(
            HWND             hWindow,
            DxvkWindowState* pState) = 0;

    virtual bool restoreDisplayMode() = 0;

    virtual HMONITOR getWindowMonitor(HWND hWindow) = 0;

    virtual bool isWindow(HWND hWindow) = 0;

    virtual bool isMinimized(HWND hWindow) = 0;

    virtual bool isOccluded(HWND hWindow) = 0;

    virtual void updateFullscreenWindow(
            HMONITOR hMonitor,
            HWND     hWindow,
            bool     forceTopmost) = 0;

    virtual VkResult createSurface(
            HWND                hWindow,
            PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
            VkInstance          instance,
            VkSurfaceKHR*       pSurface) = 0;
  };

  struct WsiBootstrap {
    const std::string name;
    bool (*createDriver)(WsiDriver **driver);
  };

#if defined(DXVK_WSI_WIN32)
  extern WsiBootstrap Win32WSI;
#endif
#if defined(DXVK_WSI_SDL3)
  extern WsiBootstrap Sdl3WSI;
#endif
#if defined(DXVK_WSI_SDL2)
  extern WsiBootstrap Sdl2WSI;
#endif
#if defined(DXVK_WSI_GLFW)
  extern WsiBootstrap GlfwWSI;
#endif

  void init();
  void quit();
  std::vector<const char *> getInstanceExtensions();

}
