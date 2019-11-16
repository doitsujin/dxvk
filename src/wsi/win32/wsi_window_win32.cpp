#include "../wsi_window.h"
#include "../wsi_mode.h"
#include "../wsi_monitor.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"

namespace dxvk::wsi {

  void getWindowSize(
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


  void resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         Width,
          uint32_t         Height) {
    // Adjust window position and size
    RECT newRect = { 0, 0, 0, 0 };
    RECT oldRect = { 0, 0, 0, 0 };
    
    ::GetWindowRect(hWindow, &oldRect);
    ::SetRect(&newRect, 0, 0, Width, Height);
    ::AdjustWindowRectEx(&newRect,
      ::GetWindowLongW(hWindow, GWL_STYLE), FALSE,
      ::GetWindowLongW(hWindow, GWL_EXSTYLE));
    ::SetRect(&newRect, 0, 0, newRect.right - newRect.left, newRect.bottom - newRect.top);
    ::OffsetRect(&newRect, oldRect.left, oldRect.top);    
    ::MoveWindow(hWindow, newRect.left, newRect.top,
        newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
  }


  bool setWindowMode(
          HMONITOR                hMonitor,
          HWND                    hWindow,
    const WsiMode*                pMode,
          bool                    EnteringFullscreen) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Win32 WSI: setWindowMode: Failed to query monitor info");
      return false;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    devMode.dmPelsWidth  = pMode->width;
    devMode.dmPelsHeight = pMode->height;
    devMode.dmBitsPerPel = pMode->bitsPerPixel;
    
    if (pMode->refreshRate.numerator != 0)  {
      devMode.dmFields |= DM_DISPLAYFREQUENCY;
      devMode.dmDisplayFrequency = pMode->refreshRate.numerator
                                 / pMode->refreshRate.denominator;
    }
    
    Logger::info(str::format("Setting display mode: ",
      devMode.dmPelsWidth, "x", devMode.dmPelsHeight, "@",
      devMode.dmDisplayFrequency));
    
    LONG status = ::ChangeDisplaySettingsExW(
      monInfo.szDevice, &devMode, nullptr, CDS_FULLSCREEN, nullptr);

    if (status != DISP_CHANGE_SUCCESSFUL && pMode->refreshRate.numerator != 0) {
      // Let's try again without display freq
      // if that failed.

      devMode.dmFields &= ~DM_DISPLAYFREQUENCY;

      status = ::ChangeDisplaySettingsExW(
      monInfo.szDevice, &devMode, nullptr, CDS_FULLSCREEN, nullptr);
    }

    if (!EnteringFullscreen && hWindow != nullptr) {
      RECT newRect = { };
      getDesktopCoordinates(hMonitor, &newRect);
      
      ::MoveWindow(hWindow, newRect.left, newRect.top,
          newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
    }
    
    return status == DISP_CHANGE_SUCCESSFUL;
  }


  bool enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             ModeSwitch) {
    // Find a display mode that matches what we need
    ::GetWindowRect(hWindow, &pState->rect);

    // Change the window flags to remove the decoration etc.
    LONG style   = ::GetWindowLongW(hWindow, GWL_STYLE);
    LONG exstyle = ::GetWindowLongW(hWindow, GWL_EXSTYLE);
    
    pState->style = style;
    pState->exstyle = exstyle;
    
    style   &= ~WS_OVERLAPPEDWINDOW;
    exstyle &= ~WS_EX_OVERLAPPEDWINDOW;
    
    ::SetWindowLongW(hWindow, GWL_STYLE, style);
    ::SetWindowLongW(hWindow, GWL_EXSTYLE, exstyle);

    RECT rect = { };
    getDesktopCoordinates(hMonitor, &rect);

    ::SetWindowPos(hWindow, HWND_TOPMOST,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);

    return true;
  }


  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState) {
    // Only restore the window style if the application hasn't
    // changed them. This is in line with what native DXGI does.
    LONG curStyle   = ::GetWindowLongW(hWindow, GWL_STYLE)   & ~WS_VISIBLE;
    LONG curExstyle = ::GetWindowLongW(hWindow, GWL_EXSTYLE) & ~WS_EX_TOPMOST;

    if (curStyle   == (pState->style   & ~(WS_VISIBLE    | WS_OVERLAPPEDWINDOW))
     && curExstyle == (pState->exstyle & ~(WS_EX_TOPMOST | WS_EX_OVERLAPPEDWINDOW))) {
      ::SetWindowLongW(hWindow, GWL_STYLE,   pState->style);
      ::SetWindowLongW(hWindow, GWL_EXSTYLE, pState->exstyle);
    }

    // Restore window position and apply the style
    const RECT rect = pState->rect;
    
    ::SetWindowPos(hWindow, 0,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
      SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);

    return true;
  }


  bool restoreDisplayMode(HMONITOR hMonitor) {
    if (!hMonitor)
      return false;

    WsiMode desktopMode = { };
    if (!getDesktopDisplayMode(hMonitor, &desktopMode))
      return false;
    
    return setWindowMode(hMonitor, nullptr, &desktopMode, false);
  }


  HMONITOR getWindowMonitor(HWND hWindow) {
    RECT windowRect = { 0, 0, 0, 0 };
    ::GetWindowRect(hWindow, &windowRect);
    
    HMONITOR monitor = ::MonitorFromPoint(
      { (windowRect.left + windowRect.right) / 2,
        (windowRect.top + windowRect.bottom) / 2 },
      MONITOR_DEFAULTTOPRIMARY);

    return monitor;
  }

}