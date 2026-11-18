#if defined(DXVK_WSI_WAYLAND)

#include "wsi_platform_wayland.h"

#include "wsi/native_wayland.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

#include <cstring>

#include <vulkan/vulkan_wayland.h>

namespace dxvk::wsi {

  static dxvk_wayland_window* fromHwnd(HWND hWindow) {
    return reinterpret_cast<dxvk_wayland_window*>(hWindow);
  }

  static constexpr uint32_t WaylandMonitorId = 1;

  // TODO dummy
  static void getWaylandMode(WsiMode* pMode) {
    pMode->width        = 1920;
    pMode->height       = 1080;
    pMode->refreshRate  = WsiRational{ 60000, 1000 };
    pMode->bitsPerPixel = 32;
    pMode->interlaced   = false;
  }


  static bool isWaylandMonitor(HMONITOR hMonitor) {
    return reinterpret_cast<uintptr_t>(hMonitor) == WaylandMonitorId;
  }


  std::vector<const char *> WaylandWsiDriver::getInstanceExtensions() {
    static const char* candidates[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
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


  HMONITOR WaylandWsiDriver::getDefaultMonitor() {
    return enumMonitors(0);
  }


  HMONITOR WaylandWsiDriver::enumMonitors(uint32_t index) {
    return index == 0
      ? reinterpret_cast<HMONITOR>(uintptr_t(WaylandMonitorId))
      : nullptr;
  }


  HMONITOR WaylandWsiDriver::enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    return enumMonitors(index);
  }


  bool WaylandWsiDriver::getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    if (!isWaylandMonitor(hMonitor))
      return false;

    std::wstring name = LR"(\\.\DISPLAY1)"; // default display name on windows
    std::memset(Name, 0, sizeof(Name));
    name.copy(Name, name.length(), 0);
    return true;
  }


  bool WaylandWsiDriver::getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    if (!isWaylandMonitor(hMonitor))
      return false;

    WsiMode mode;
    getWaylandMode(&mode);

    pRect->left   = 0;
    pRect->top    = 0;
    pRect->right  = LONG(mode.width);
    pRect->bottom = LONG(mode.height);
    return true;
  }


  bool WaylandWsiDriver::getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         modeNumber,
          WsiMode*         pMode) {
    if (!isWaylandMonitor(hMonitor) || modeNumber != 0)
      return false;

    getWaylandMode(pMode);
    return true;
  }


  bool WaylandWsiDriver::getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return getDisplayMode(hMonitor, 0, pMode);
  }


  bool WaylandWsiDriver::getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return getDisplayMode(hMonitor, 0, pMode);
  }


  WsiEdidData WaylandWsiDriver::getMonitorEdid(HMONITOR hMonitor) {
    return {};
  }


  void WaylandWsiDriver::getWindowSize(
          HWND      hWindow,
          uint32_t* pWidth,
          uint32_t* pHeight) {
    const dxvk_wayland_window* window = fromHwnd(hWindow);

    if (pWidth)
      *pWidth = window ? window->width : 0;

    if (pHeight)
      *pHeight = window ? window->height : 0;
  }


  void WaylandWsiDriver::resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         width,
          uint32_t         height) {
  }

  bool WaylandWsiDriver::setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
    const WsiMode&         mode) {
    return false;
  }


  bool WaylandWsiDriver::enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             modeSwitch) {
    return false;
  }


  void WaylandWsiDriver::saveWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             saveStyle) {
  }


  void WaylandWsiDriver::restoreWindowState(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
  }


  bool WaylandWsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
    return true;
  }


  bool WaylandWsiDriver::restoreDisplayMode() {
    return true;
  }


  HMONITOR WaylandWsiDriver::getWindowMonitor(HWND hWindow) {
    return getDefaultMonitor();
  }


  bool WaylandWsiDriver::isWindow(HWND hWindow) {
    return fromHwnd(hWindow) != nullptr;
  }


  bool WaylandWsiDriver::isMinimized(HWND hWindow) {
    const dxvk_wayland_window* window = fromHwnd(hWindow);
    return window != nullptr && window->minimized;
  }


  bool WaylandWsiDriver::isOccluded(HWND hWindow) {
    return false;
  }


  void WaylandWsiDriver::updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {

  }


  VkResult WaylandWsiDriver::createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance                instance,
          VkSurfaceKHR*             pSurface) {
    const dxvk_wayland_window* window = fromHwnd(hWindow);

    if (!window)
      return VK_ERROR_INITIALIZATION_FAILED;

    auto pfnCreateSurface = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
      pfnVkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));

    if (!pfnCreateSurface) {
      Logger::err("Wayland WSI: vkCreateWaylandSurfaceKHR not available");
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkWaylandSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
    info.display = window->display;
    info.surface = window->surface;
    return pfnCreateSurface(instance, &info, nullptr, pSurface);
  }


  static bool createWaylandWsiDriver(WsiDriver **driver) {
    *driver = new WaylandWsiDriver();
    return true;
  }

  WsiBootstrap WaylandWSI = {
    "Wayland",
    createWaylandWsiDriver
  };

}

#endif
