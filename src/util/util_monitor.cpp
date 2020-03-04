#include "util_monitor.h"

#include "./log/log.h"

namespace dxvk {
  
  HMONITOR GetDefaultMonitor() {
    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
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
