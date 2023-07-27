#include "../wsi_window.h"

#include "native/wsi/native_wsi.h"
#include "wsi_platform_none.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

namespace dxvk::wsi {

  void getWindowSize(
        HWND      hWindow,
        uint32_t* pWidth,
        uint32_t* pHeight) {
  }


  void resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         Width,
          uint32_t         Height) {
  }


  bool setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
    const WsiMode&         pMode) {
    return false;
  }



  bool enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             ModeSwitch) {
    return false;
  }


  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    return false;
  }


  bool restoreDisplayMode() {
    return false;
  }


  HMONITOR getWindowMonitor(HWND hWindow) {
    return nullptr;
  }


  bool isWindow(HWND hWindow) {
    return false;
  }


  void updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {
  }


  VkResult createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    auto pfnVkCreateHeadlessSurfaceEXT = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
      pfnVkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT"));

    if (!pfnVkCreateHeadlessSurfaceEXT)
      return VK_ERROR_FEATURE_NOT_PRESENT;

    VkHeadlessSurfaceCreateInfoEXT info = { VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT };
    info.pNext = nullptr;
    info.flags = 0;
    return pfnVkCreateHeadlessSurfaceEXT(instance, &info, nullptr, pSurface);
  }

}
