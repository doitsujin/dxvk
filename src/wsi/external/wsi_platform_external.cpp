#if defined(DXVK_WSI_EXTERNAL)

#include "wsi_platform_external.h"

#include "wsi/native_external_wsi.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <cstring>

// declarations to avoid needing to pull x/wayland headers
typedef struct _XDisplay Display;
typedef unsigned long    Window;
typedef unsigned long    VisualID;
typedef struct xcb_connection_t xcb_connection_t;
typedef uint32_t         xcb_window_t;
typedef uint32_t         xcb_visualid_t;
struct wl_display;
struct wl_surface;

#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>

namespace dxvk::wsi {

  static constexpr uint32_t ExternalMonitorId = 1;

  // TODO dummy
  static void getExternalMode(WsiMode* pMode) {
    pMode->width        = 1920;
    pMode->height       = 1080;
    pMode->refreshRate  = WsiRational{ 60000, 1000 };
    pMode->bitsPerPixel = 32;
    pMode->interlaced   = false;
  }


  static bool isExternalMonitor(HMONITOR hMonitor) {
    return reinterpret_cast<uintptr_t>(hMonitor) == ExternalMonitorId;
  }


  std::vector<const char *> ExternalWsiDriver::getInstanceExtensions() {
    static const char* candidates[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      "VK_KHR_xlib_surface",
      "VK_KHR_xcb_surface",
      "VK_KHR_wayland_surface",
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


  HMONITOR ExternalWsiDriver::getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR ExternalWsiDriver::enumMonitors(uint32_t index) {
    return index == 0
      ? reinterpret_cast<HMONITOR>(uintptr_t(ExternalMonitorId))
      : nullptr;
  }


  HMONITOR ExternalWsiDriver::enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }


  bool ExternalWsiDriver::getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    if (!isExternalMonitor(hMonitor))
      return false;

    std::wstring name = LR"(\\.\DISPLAY1)"; // default display name on windows
    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);
    return true;
  }


  bool ExternalWsiDriver::getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    if (!isExternalMonitor(hMonitor))
      return false;

    WsiMode mode;
    getExternalMode(&mode);

    pRect->left   = 0;
    pRect->top    = 0;
    pRect->right  = LONG(mode.width);
    pRect->bottom = LONG(mode.height);
    return true;
  }


  bool ExternalWsiDriver::getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         modeNumber,
          WsiMode*         pMode) {
    if (!isExternalMonitor(hMonitor) || modeNumber != 0)
      return false;

    getExternalMode(pMode);
    return true;
  }


  bool ExternalWsiDriver::getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return getDisplayMode(hMonitor, 0, pMode);
  }


  bool ExternalWsiDriver::getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return getDisplayMode(hMonitor, 0, pMode);
  }


  WsiEdidData ExternalWsiDriver::getMonitorEdid(HMONITOR hMonitor) {
    return {};
  }


  void ExternalWsiDriver::getWindowSize(
          HWND      hWindow,
          uint32_t* pWidth,
          uint32_t* pHeight) {
    const dxvk_external_window* window = fromHwnd(hWindow);

    if (pWidth)
      *pWidth = window ? window->width : 0;

    if (pHeight)
      *pHeight = window ? window->height : 0;
  }


  void ExternalWsiDriver::resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         width,
          uint32_t         height) {
  }

  bool ExternalWsiDriver::setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
    const WsiMode&         mode) {
    return false;
  }


  bool ExternalWsiDriver::enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             modeSwitch) {
    return false;
  }


  void ExternalWsiDriver::saveWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             saveStyle) {
  }


  void ExternalWsiDriver::restoreWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
  }


  bool ExternalWsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
    return true;
  }


  bool ExternalWsiDriver::restoreDisplayMode() {
    return true;
  }


  HMONITOR ExternalWsiDriver::getWindowMonitor(HWND hWindow) {
    return getDefaultMonitor();
  }


  bool ExternalWsiDriver::isWindow(HWND hWindow) {
    return fromHwnd(hWindow) != nullptr;
  }


  bool ExternalWsiDriver::isMinimized(HWND hWindow) {
    const dxvk_external_window* window = fromHwnd(hWindow);
    return window !=nullptr && window->minimized;
  }


  bool ExternalWsiDriver::isOccluded(HWND hWindow) {
    return false;
  }


  void ExternalWsiDriver::updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {

  }


  VkResult ExternalWsiDriver::createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    const dxvk_external_window* window = fromHwnd(hWindow);

    if (!window)
      return VK_ERROR_INITIALIZATION_FAILED;

    switch (window->type) {
      case DXVK_EXTERNAL_WINDOW_XLIB: {
        auto pfnCreateSurface = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
          pfnVkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));

        if (!pfnCreateSurface) {
          Logger::err("External WSI: vkCreateXlibSurfaceKHR not available");
          return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkXlibSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
        info.dpy    = static_cast<Display*>(window->display);
        info.window = static_cast<Window>(window->window);
        return pfnCreateSurface(instance, &info, nullptr, pSurface);
      }

      case DXVK_EXTERNAL_WINDOW_XCB: {
        auto pfnCreateSurface = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
          pfnVkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));

        if (!pfnCreateSurface) {
          Logger::err("External WSI: vkCreateXcbSurfaceKHR not available");
          return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkXcbSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR };
        info.connection = static_cast<xcb_connection_t*>(window->display);
        info.window     = static_cast<xcb_window_t>(window->window);
        return pfnCreateSurface(instance, &info, nullptr, pSurface);
      }

      case DXVK_EXTERNAL_WINDOW_WAYLAND: {
        auto pfnCreateSurface = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
          pfnVkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));

        if (!pfnCreateSurface) {
          Logger::err("External WSI: vkCreateWaylandSurfaceKHR not available");
          return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkWaylandSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
        info.display = static_cast<wl_display*>(window->display);
        info.surface = reinterpret_cast<wl_surface*>(window->window);
        return pfnCreateSurface(instance, &info, nullptr, pSurface);
      }

      default:
        Logger::err(str::format("External WSI: Unknown window type ", window->type));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
  }


  static bool createExternalWsiDriver(WsiDriver **driver) {
    *driver = new ExternalWsiDriver();
    return true;
  }

  WsiBootstrap ExternalWSI = {
    "External",
    createExternalWsiDriver
  };

}

#endif
