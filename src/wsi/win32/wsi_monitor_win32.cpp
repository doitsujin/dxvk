#include "../wsi_monitor.h"

#include "../../util/log/log.h"

#include <cstring>

namespace dxvk::wsi {

  HMONITOR enumMonitors(uint32_t index) {
    // TODO: Multi monitor support.
    if (index > 0)
      return nullptr;

    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
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

}