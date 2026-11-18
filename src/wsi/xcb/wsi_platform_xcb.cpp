#if defined(DXVK_WSI_XCB)

#include "wsi_platform_xcb.h"

#include "wsi/native_xcb.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <cstring>

// declarations to avoid needing to pull xcb headers
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_visualid_t;

#include <vulkan/vulkan_xcb.h>

namespace dxvk::wsi {

  static dxvk_xcb_window* fromHwnd(HWND hWindow) {
    return reinterpret_cast<dxvk_xcb_window*>(hWindow);
  }

  static constexpr uint32_t XcbMonitorId = 1;

  // TODO dummy
  static void getXcbMode(WsiMode* pMode) {
    pMode->width        = 1920;
    pMode->height       = 1080;
    pMode->refreshRate  = WsiRational{ 60000, 1000 };
    pMode->bitsPerPixel = 32;
    pMode->interlaced   = false;
  }


  static bool isXcbMonitor(HMONITOR hMonitor) {
    return reinterpret_cast<uintptr_t>(hMonitor) == XcbMonitorId;
  }


  std::vector<const char *> XcbWsiDriver::getInstanceExtensions() {
    static const char* candidates[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      "VK_KHR_xcb_surface",
    };

    Rc<vk::LibraryFn> vkl = new vk::LibraryFn();

    std::vector<VkExtensionProperties> available;

    if (vkl->vkEnumerateInstanceExtensionProperties) {
      uint32_t count = 0;
      vkl->vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
      available.resize(count);
      vkl->vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());
      available.resize(count);
    }

    std::vector<const char*> result;

    for (const char* name : candidates) {
      for (const auto& ext : available) {
        if (!std::strcmp(ext.extensionName, name)) {
          result.push_back(name);
          break;
        }
      }
    }

    return result;
  }


  HMONITOR XcbWsiDriver::getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR XcbWsiDriver::enumMonitors(uint32_t index) {
    return index == 0
      ? reinterpret_cast<HMONITOR>(uintptr_t(XcbMonitorId))
      : nullptr;
  }


  HMONITOR XcbWsiDriver::enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }


  bool XcbWsiDriver::getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    if (!isXcbMonitor(hMonitor))
      return false;

    std::wstring name = LR"(\\.\DISPLAY1)"; // default display name on windows
    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);
    return true;
  }


  bool XcbWsiDriver::getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    if (!isXcbMonitor(hMonitor))
      return false;

    WsiMode mode;
    getXcbMode(&mode);

    pRect->left   = 0;
    pRect->top    = 0;
    pRect->right  = LONG(mode.width);
    pRect->bottom = LONG(mode.height);
    return true;
  }


  bool XcbWsiDriver::getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         modeNumber,
          WsiMode*         pMode) {
    if (!isXcbMonitor(hMonitor) || modeNumber != 0)
      return false;

    getXcbMode(pMode);
    return true;
  }


  bool XcbWsiDriver::getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return getDisplayMode(hMonitor, 0, pMode);
  }


  bool XcbWsiDriver::getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return getDisplayMode(hMonitor, 0, pMode);
  }


  WsiEdidData XcbWsiDriver::getMonitorEdid(HMONITOR hMonitor) {
    return {};
  }


  void XcbWsiDriver::getWindowSize(
          HWND      hWindow,
          uint32_t* pWidth,
          uint32_t* pHeight) {
    const dxvk_xcb_window* window = fromHwnd(hWindow);

    if (pWidth)
      *pWidth = window ? window->width : 0;

    if (pHeight)
      *pHeight = window ? window->height : 0;
  }


  void XcbWsiDriver::resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         width,
          uint32_t         height) {
  }

  bool XcbWsiDriver::setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
    const WsiMode&         mode) {
    return false;
  }


  bool XcbWsiDriver::enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             modeSwitch) {
    return false;
  }


  void XcbWsiDriver::saveWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             saveStyle) {
  }


  void XcbWsiDriver::restoreWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
  }


  bool XcbWsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
    return true;
  }


  bool XcbWsiDriver::restoreDisplayMode() {
    return true;
  }


  HMONITOR XcbWsiDriver::getWindowMonitor(HWND hWindow) {
    return getDefaultMonitor();
  }


  bool XcbWsiDriver::isWindow(HWND hWindow) {
    return fromHwnd(hWindow) != nullptr;
  }


  bool XcbWsiDriver::isMinimized(HWND hWindow) {
    const dxvk_xcb_window* window = fromHwnd(hWindow);
    return window != nullptr && window->minimized;
  }


  bool XcbWsiDriver::isOccluded(HWND hWindow) {
    return false;
  }


  void XcbWsiDriver::updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {

  }


  VkResult XcbWsiDriver::createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    const dxvk_xcb_window* window = fromHwnd(hWindow);

    if (!window)
      return VK_ERROR_INITIALIZATION_FAILED;

    auto pfnCreateSurface = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
      pfnVkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));

    if (!pfnCreateSurface) {
      Logger::err("XCB WSI: vkCreateXcbSurfaceKHR not available");
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkXcbSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
    info.connection = window->connection;
    info.window     = window->window;
    return pfnCreateSurface(instance, &info, nullptr, pSurface);
  }


  static bool createXcbWsiDriver(WsiDriver **driver) {
    *driver = new XcbWsiDriver();
    return true;
  }

  WsiBootstrap XcbWSI = {
    "XCB",
    createXcbWsiDriver
  };

}

#endif
