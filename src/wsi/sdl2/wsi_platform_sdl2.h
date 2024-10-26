#pragma once

#include <SDL.h>

#include "../wsi_platform.h"

namespace dxvk::wsi {

  class Sdl2WsiDriver : public WsiDriver {
  private:
    HMODULE libsdl;
    #define SDL_PROC(ret, name, params) \
      typedef ret (SDLCALL *pfn_##name) params; \
      pfn_##name name;
    #include "wsi_platform_sdl2_funcs.h"

    inline bool isDisplayValid(int32_t displayId) {
      const int32_t displayCount = SDL_GetNumVideoDisplays();

      return displayId < displayCount && displayId >= 0;
    }

  public:
    Sdl2WsiDriver();
    ~Sdl2WsiDriver();

    // Platform
    virtual std::vector<const char *> getInstanceExtensions();

    // Monitor
    virtual HMONITOR getDefaultMonitor();

    virtual HMONITOR enumMonitors(uint32_t index);

    virtual HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index);

    virtual bool getDisplayName(
            HMONITOR         hMonitor,
            WCHAR            (&Name)[32]);

    virtual bool getDesktopCoordinates(
            HMONITOR         hMonitor,
            RECT*            pRect);

    virtual bool getDisplayMode(
            HMONITOR         hMonitor,
            uint32_t         modeNumber,
            WsiMode*         pMode);

    virtual bool getCurrentDisplayMode(
            HMONITOR         hMonitor,
            WsiMode*         pMode);

    virtual bool getDesktopDisplayMode(
            HMONITOR         hMonitor,
            WsiMode*         pMode);

    virtual WsiEdidData getMonitorEdid(HMONITOR hMonitor);

    // Window

    virtual void getWindowSize(
            HWND      hWindow,
            uint32_t* pWidth,
            uint32_t* pWeight);

    virtual void resizeWindow(
            HWND             hWindow,
            DxvkWindowState* pState,
            uint32_t         width,
            uint32_t         weight);

    virtual bool setWindowMode(
            HMONITOR         hMonitor,
            HWND             hWindow,
            DxvkWindowState* pState,
      const WsiMode&         mode);

    virtual bool enterFullscreenMode(
            HMONITOR         hMonitor,
            HWND             hWindow,
            DxvkWindowState* pState,
            [[maybe_unused]]
            bool             modeSwitch);

    virtual bool leaveFullscreenMode(
            HWND             hWindow,
            DxvkWindowState* pState,
            bool             restoreCoordinates);

    virtual bool restoreDisplayMode();

    virtual HMONITOR getWindowMonitor(HWND hWindow);

    virtual bool isWindow(HWND hWindow);

    virtual bool isMinimized(HWND hWindow);

    virtual bool isOccluded(HWND hWindow);

    virtual void updateFullscreenWindow(
            HMONITOR hMonitor,
            HWND     hWindow,
            bool     forceTopmost);

    virtual VkResult createSurface(
            HWND                hWindow,
            PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
            VkInstance          instance,
            VkSurfaceKHR*       pSurface);
  };
  
}
