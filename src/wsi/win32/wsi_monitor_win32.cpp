#include "../wsi_monitor.h"

#include "../../util/log/log.h"

#include <cstring>

namespace dxvk::wsi {

  HMONITOR getDefaultMonitor() {
    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
  }

  struct MonitorEnumInfo {
    UINT      iMonitorId;
    HMONITOR  oMonitor;
  };

  static BOOL CALLBACK MonitorEnumProc(
          HMONITOR                  hmon,
          HDC                       hdc,
          LPRECT                    rect,
          LPARAM                    lp) {
    auto data = reinterpret_cast<MonitorEnumInfo*>(lp);

    if (data->iMonitorId--)
      return TRUE; /* continue */

    data->oMonitor = hmon;
    return FALSE; /* stop */
  }

  HMONITOR enumMonitors(uint32_t index) {
    MonitorEnumInfo info;
    info.iMonitorId = index;
    info.oMonitor   = nullptr;

    ::EnumDisplayMonitors(
      nullptr, nullptr, &MonitorEnumProc,
      reinterpret_cast<LPARAM>(&info));

    return info.oMonitor;
  }


  bool getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]) {
    // Query monitor info to get the device name
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Win32 WSI: getDisplayName: Failed to query monitor info");
      return false;
    }

    std::memcpy(Name, monInfo.szDevice, sizeof(Name));

    return true;
  }


  bool getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Win32 WSI: getDisplayName: Failed to query monitor info");
      return false;
    }

    *pRect = monInfo.rcMonitor;

    return true;
  }

  static inline void convertMode(const DEVMODEW& mode, WsiMode* pMode) {
    pMode->width         = mode.dmPelsWidth;
    pMode->height        = mode.dmPelsHeight;
    pMode->refreshRate   = WsiRational{ mode.dmDisplayFrequency * 1000, 1000 }; 
    pMode->bitsPerPixel  = mode.dmBitsPerPel;
    pMode->interlaced    = mode.dmDisplayFlags & DM_INTERLACED;
  }


  static inline bool retrieveDisplayMode(
          HMONITOR         hMonitor,
          DWORD            modeNumber,
          WsiMode*         pMode) {
    // Query monitor info to get the device name
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Win32 WSI: retrieveDisplayMode: Failed to query monitor info");
      return false;
    }

    DEVMODEW devMode = { };
    devMode.dmSize = sizeof(devMode);
    
    if (!::EnumDisplaySettingsW(monInfo.szDevice, modeNumber, &devMode))
      return false;

    convertMode(devMode, pMode);

    return true;
  }


  bool getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         modeNumber,
          WsiMode*         pMode) {
    return retrieveDisplayMode(hMonitor, modeNumber, pMode);
  }


  bool getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return retrieveDisplayMode(hMonitor, ENUM_CURRENT_SETTINGS, pMode);
  }


  bool getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode) {
    return retrieveDisplayMode(hMonitor, ENUM_REGISTRY_SETTINGS, pMode);
  }

}