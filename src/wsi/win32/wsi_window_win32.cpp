#if defined(DXVK_WSI_WIN32)

#include "wsi_platform_win32.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

namespace dxvk::wsi {

  static bool getMonitorDisplayMode(
          HMONITOR                hMonitor,
          DWORD                   modeNum,
          DEVMODEW*               pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Failed to query monitor info");
      return false;
    }

    return ::EnumDisplaySettingsW(monInfo.szDevice, modeNum, pMode);
  }


  static bool setMonitorDisplayMode(
          HMONITOR                hMonitor,
          DEVMODEW*               pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Failed to query monitor info");
      return false;
    }

    Logger::info(str::format("Setting display mode: ",
      pMode->dmPelsWidth, "x", pMode->dmPelsHeight, "@",
      pMode->dmDisplayFrequency));

    DEVMODEW curMode = { };
    curMode.dmSize = sizeof(curMode);

    if (getMonitorDisplayMode(hMonitor, ENUM_CURRENT_SETTINGS, &curMode)) {
      bool eq = curMode.dmPelsWidth  == pMode->dmPelsWidth
             && curMode.dmPelsHeight == pMode->dmPelsHeight
             && curMode.dmBitsPerPel == pMode->dmBitsPerPel;

      if (pMode->dmFields & DM_DISPLAYFREQUENCY)
        eq &= curMode.dmDisplayFrequency == pMode->dmDisplayFrequency;
      if (pMode->dmFields & DM_DISPLAYFLAGS)
        eq &= curMode.dmDisplayFlags == pMode->dmDisplayFlags;
      if (pMode->dmFields & DM_DISPLAYORIENTATION)
        eq &= curMode.dmDisplayOrientation == pMode->dmDisplayOrientation;
      if (pMode->dmFields & DM_POSITION)
        eq &= curMode.dmPosition.x == pMode->dmPosition.x && curMode.dmPosition.y == pMode->dmPosition.y;

      if (eq)
        return true;
    }

    LONG status = ::ChangeDisplaySettingsExW(monInfo.szDevice,
      pMode, nullptr, CDS_FULLSCREEN, nullptr);

    if (status != DISP_CHANGE_SUCCESSFUL) {
      pMode->dmFields &= ~DM_DISPLAYFREQUENCY;

      status = ::ChangeDisplaySettingsExW(monInfo.szDevice,
        pMode, nullptr, CDS_FULLSCREEN, nullptr);
    }

    return status == DISP_CHANGE_SUCCESSFUL;
  }


  static BOOL CALLBACK restoreDisplayModeCallback(
          HMONITOR                hMonitor,
          HDC                     hDC,
          LPRECT                  pRect,
          LPARAM                  pUserdata) {
    auto success = reinterpret_cast<bool*>(pUserdata);

    DEVMODEW devMode = { };
    devMode.dmSize = sizeof(devMode);

    if (!getMonitorDisplayMode(hMonitor, ENUM_REGISTRY_SETTINGS, &devMode)) {
      *success = false;
      return false;
    }

    Logger::info(str::format("Restoring display mode: ",
      devMode.dmPelsWidth, "x", devMode.dmPelsHeight, "@",
      devMode.dmDisplayFrequency));

    if (!setMonitorDisplayMode(hMonitor, &devMode)) {
      *success = false;
      return false;
    }

    return true;
  }


  void Win32WsiDriver::getWindowSize(
        HWND      hWindow,
        uint32_t* pWidth,
        uint32_t* pHeight) {
    RECT rect = { };
    ::GetClientRect(hWindow, &rect);
    
    if (pWidth)
      *pWidth = rect.right - rect.left;
    
    if (pHeight)
      *pHeight = rect.bottom - rect.top;
  }


  void Win32WsiDriver::resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         width,
          uint32_t         height) {
    // Adjust window position and size
    RECT newRect = { 0, 0, 0, 0 };
    RECT oldRect = { 0, 0, 0, 0 };
    
    ::GetWindowRect(hWindow, &oldRect);
    ::SetRect(&newRect, 0, 0, width, height);
    ::AdjustWindowRectEx(&newRect,
      ::GetWindowLongW(hWindow, GWL_STYLE), FALSE,
      ::GetWindowLongW(hWindow, GWL_EXSTYLE));
    ::SetRect(&newRect, 0, 0, newRect.right - newRect.left, newRect.bottom - newRect.top);
    ::OffsetRect(&newRect, oldRect.left, oldRect.top);    
    ::MoveWindow(hWindow, newRect.left, newRect.top,
        newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
  }


  bool Win32WsiDriver::setWindowMode(
          HMONITOR                hMonitor,
          HWND                    hWindow,
          DxvkWindowState*        pState,
    const WsiMode&                mode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Win32 WSI: setWindowMode: Failed to query monitor info");
      return false;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    devMode.dmPelsWidth  = mode.width;
    devMode.dmPelsHeight = mode.height;
    devMode.dmBitsPerPel = mode.bitsPerPixel;
    
    if (mode.refreshRate.numerator != 0)  {
      devMode.dmFields |= DM_DISPLAYFREQUENCY;
      devMode.dmDisplayFrequency = mode.refreshRate.numerator
                                 / mode.refreshRate.denominator;
    }
    
    Logger::info(str::format("Setting display mode: ",
      devMode.dmPelsWidth, "x", devMode.dmPelsHeight, "@",
      devMode.dmDisplayFrequency));
    
    return setMonitorDisplayMode(hMonitor, &devMode);
  }


  bool Win32WsiDriver::enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          [[maybe_unused]]
          bool             modeSwitch) {
    // Find a display mode that matches what we need
    ::GetWindowRect(hWindow, &pState->win.rect);

    // Change the window flags to remove the decoration etc.
    LONG style   = ::GetWindowLongW(hWindow, GWL_STYLE);
    LONG exstyle = ::GetWindowLongW(hWindow, GWL_EXSTYLE);
    
    pState->win.style = style;
    pState->win.exstyle = exstyle;
    
    style   &= ~WS_OVERLAPPEDWINDOW;
    exstyle &= ~WS_EX_OVERLAPPEDWINDOW;
    
    ::SetWindowLongW(hWindow, GWL_STYLE, style);
    ::SetWindowLongW(hWindow, GWL_EXSTYLE, exstyle);

    RECT rect = { };
    getDesktopCoordinates(hMonitor, &rect);

    ::SetWindowPos(hWindow, HWND_TOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);

    m_lastForegroundTimestamp = 0;
    return true;
  }


  bool Win32WsiDriver::leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates) {
    // Only restore the window style if the application hasn't
    // changed them. This is in line with what native DXGI does.
    LONG curStyle   = ::GetWindowLongW(hWindow, GWL_STYLE)   & ~WS_VISIBLE;
    LONG curExstyle = ::GetWindowLongW(hWindow, GWL_EXSTYLE) & ~WS_EX_TOPMOST;

    if (curStyle   == (pState->win.style   & ~(WS_VISIBLE    | WS_OVERLAPPEDWINDOW))
     && curExstyle == (pState->win.exstyle & ~(WS_EX_TOPMOST | WS_EX_OVERLAPPEDWINDOW))) {
      ::SetWindowLongW(hWindow, GWL_STYLE,   pState->win.style);
      ::SetWindowLongW(hWindow, GWL_EXSTYLE, pState->win.exstyle);
    }

    // Restore window position and apply the style
    UINT flags = SWP_FRAMECHANGED | SWP_NOACTIVATE;
    const RECT rect = pState->win.rect;

    if (!restoreCoordinates)
      flags |= SWP_NOSIZE | SWP_NOMOVE;
    
    ::SetWindowPos(hWindow, (pState->win.exstyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, flags);

    return true;
  }


  bool Win32WsiDriver::restoreDisplayMode() {
    bool success = true;
    bool result = ::EnumDisplayMonitors(nullptr, nullptr,
      &restoreDisplayModeCallback,
      reinterpret_cast<LPARAM>(&success));

    return result && success;
  }


  HMONITOR Win32WsiDriver::getWindowMonitor(HWND hWindow) {
    RECT windowRect = { 0, 0, 0, 0 };
    ::GetWindowRect(hWindow, &windowRect);
    
    HMONITOR monitor = ::MonitorFromPoint(
      { (windowRect.left + windowRect.right) / 2,
        (windowRect.top + windowRect.bottom) / 2 },
      MONITOR_DEFAULTTOPRIMARY);

    return monitor;
  }


  bool Win32WsiDriver::isWindow(HWND hWindow) {
    return ::IsWindow(hWindow);
  }


  bool Win32WsiDriver::isMinimized(HWND hWindow) {
    return (::GetWindowLongW(hWindow, GWL_STYLE) & WS_MINIMIZE) != 0;
  }


  bool Win32WsiDriver::isOccluded(HWND hWindow) {
    if (::GetForegroundWindow() == hWindow)
    {
      m_lastForegroundTimestamp = GetTickCount64();
      return false;
    }
    return m_lastForegroundTimestamp && GetTickCount64() - m_lastForegroundTimestamp > 100;
  }


  void Win32WsiDriver::updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost) {
    RECT bounds = { };
    wsi::getDesktopCoordinates(hMonitor, &bounds);

    // In D3D9, changing display modes re-forces the window
    // to become top most, whereas in DXGI, it does not.
    if (forceTopmost) {
      ::SetWindowPos(hWindow, HWND_TOPMOST,
        bounds.left, bounds.top,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    } else {
      ::MoveWindow(hWindow, bounds.left, bounds.top,
        bounds.right - bounds.left, bounds.bottom - bounds.top, TRUE);
    }
  }


  VkResult Win32WsiDriver::createSurface(
          HWND                hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance          instance,
          VkSurfaceKHR*       pSurface) {
    HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(hWindow, GWLP_HINSTANCE));

    auto pfnVkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
      pfnVkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));

    if (!pfnVkCreateWin32SurfaceKHR)
      return VK_ERROR_FEATURE_NOT_PRESENT;

    VkWin32SurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    info.hinstance  = hInstance;
    info.hwnd       = hWindow;
    
    return (*pfnVkCreateWin32SurfaceKHR)(instance, &info, nullptr, pSurface);
  }

}

#endif
