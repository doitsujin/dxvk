#pragma once

#include "../../vulkan/vulkan_loader.h"
#include <GLFW/glfw3.h>

#include "../wsi_platform.h"

namespace dxvk::wsi {

  class GlfwWsiDriver : public WsiDriver {
  private:
    HMODULE libglfw;
    #define GLFW_PROC(ret, name, params) \
      typedef ret (*pfn_##name) params; \
      pfn_##name name;
    #include "wsi_platform_glfw_funcs.h"

    inline bool isDisplayValid(int32_t displayId) {
      int32_t displayCount = 0;
      glfwGetMonitors(&displayCount);

      return displayId < displayCount && displayId >= 0;
    }

  public:
    GlfwWsiDriver();
    ~GlfwWsiDriver();

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
