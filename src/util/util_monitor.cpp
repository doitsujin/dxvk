#include "util_monitor.h"
#include "util_string.h"

#include "./log/log.h"

namespace dxvk {
  
  HMONITOR GetDefaultMonitor() {
    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
  }


  BOOL SetMonitorDisplayMode(
          HMONITOR                hMonitor,
          DEVMODEW*               pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Failed to query monitor info");
      return E_FAIL;
    }

    Logger::info(str::format("Setting display mode: ",
      pMode->dmPelsWidth, "x", pMode->dmPelsHeight, "@",
      pMode->dmDisplayFrequency));

    DEVMODEW curMode = { };
    curMode.dmSize = sizeof(curMode);

    if (GetMonitorDisplayMode(hMonitor, ENUM_CURRENT_SETTINGS, &curMode)) {
      bool eq = curMode.dmPelsWidth  == pMode->dmPelsWidth
             && curMode.dmPelsHeight == pMode->dmPelsHeight
             && curMode.dmBitsPerPel == pMode->dmBitsPerPel;

      if (pMode->dmFields & DM_DISPLAYFREQUENCY)
        eq &= curMode.dmDisplayFrequency == pMode->dmDisplayFrequency;

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


  BOOL GetMonitorDisplayMode(
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


  BOOL CALLBACK RestoreMonitorDisplayModeCallback(
          HMONITOR                hMonitor,
          HDC                     hDC,
          LPRECT                  pRect,
          LPARAM                  pUserdata) {
    auto success = reinterpret_cast<bool*>(pUserdata);

    DEVMODEW devMode = { };
    devMode.dmSize = sizeof(devMode);

    if (!GetMonitorDisplayMode(hMonitor, ENUM_REGISTRY_SETTINGS, &devMode)) {
      *success = false;
      return false;
    }

    Logger::info(str::format("Restoring display mode: ",
      devMode.dmPelsWidth, "x", devMode.dmPelsHeight, "@",
      devMode.dmDisplayFrequency));

    if (!SetMonitorDisplayMode(hMonitor, &devMode)) {
      *success = false;
      return false;
    }

    return true;
  }


  BOOL RestoreMonitorDisplayMode() {
    bool success = true;
    bool result = ::EnumDisplayMonitors(nullptr, nullptr,
      &RestoreMonitorDisplayModeCallback,
      reinterpret_cast<LPARAM>(&success));

    return result && success;
  }


  void GetWindowClientSize(
          HWND                    hWnd,
          UINT*                   pWidth,
          UINT*                   pHeight) {
    RECT rect = { };
    ::GetClientRect(hWnd, &rect);
    
    if (pWidth)
      *pWidth = rect.right - rect.left;
    
    if (pHeight)
      *pHeight = rect.bottom - rect.top;
  }


  void GetMonitorClientSize(
          HMONITOR                hMonitor,
          UINT*                   pWidth,
          UINT*                   pHeight) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Failed to query monitor info");
      return;
    }
    
    auto rect = monInfo.rcMonitor;

    if (pWidth)
      *pWidth = rect.right - rect.left;
    
    if (pHeight)
      *pHeight = rect.bottom - rect.top;
  }


  void GetMonitorRect(
          HMONITOR                hMonitor,
          RECT*                   pRect) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Failed to query monitor info");
      return;
    }

    *pRect = monInfo.rcMonitor;
  }

}
