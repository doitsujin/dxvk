#pragma once

#include <SDL3/SDL.h>

#include "../wsi_platform.h"

#include "../util/util_bit.h"

namespace dxvk::wsi {

  class Sdl3WsiDriver : public WsiDriver {
  private:
    HMODULE libsdl;
    #define SDL_PROC(ret, name, params) \
      typedef ret (SDLCALL *pfn_##name) params; \
      pfn_##name name;
    #include "wsi_platform_sdl3_funcs.h"

    static void convertMode(const SDL_DisplayMode& mode, WsiMode* pMode) {
      pMode->width          = uint32_t(mode.w);
      pMode->height         = uint32_t(mode.h);
      if (mode.refresh_rate_numerator) {
        pMode->refreshRate  = WsiRational {
          uint32_t(mode.refresh_rate_numerator),
          uint32_t(mode.refresh_rate_denominator) };
      } else if (mode.refresh_rate > 0.0f) {
        pMode->refreshRate  = WsiRational {
          uint32_t(mode.refresh_rate * 1000.0f),
          1000 };
      } else {
        // Platform gave us no refresh rate to work with, assume 60Hz :(
        pMode->refreshRate  = WsiRational { 60, 1 };
      }
      // BPP should always be a power of two
      // to match Windows behaviour of including padding.
      pMode->bitsPerPixel   = (uint32_t(-1) >> bit::lzcnt(uint32_t(SDL_BITSPERPIXEL(mode.format) - 1u))) + 1u;
      pMode->interlaced     = false;
    }
  public:
    Sdl3WsiDriver();
    ~Sdl3WsiDriver();

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
