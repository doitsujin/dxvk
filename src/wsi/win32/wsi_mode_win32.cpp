#include "../wsi_mode.h"

#include "../../util/log/log.h"

namespace dxvk::wsi {

  static inline void convertMode(const DEVMODEW& mode, WsiMode* pMode) {
    pMode->width         = mode.dmPelsWidth;
    pMode->height        = mode.dmPelsHeight;
    pMode->refreshRate   = WsiRational{ mode.dmDisplayFrequency * 1000, 1000 }; 
    pMode->bitsPerPixel  = mode.dmBitsPerPel;
    pMode->interlaced    = mode.dmDisplayFlags & DM_INTERLACED;
  }


  static inline bool retrieveDisplayMode(
          HMONITOR         hMonitor,
          DWORD            ModeNumber,
          WsiMode*        pMode) {
    // Query monitor info to get the device name
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("Win32 WSI: retrieveDisplayMode: Failed to query monitor info");
      return false;
    }

    DEVMODEW devMode = { };
    devMode.dmSize = sizeof(devMode);
    
    if (!::EnumDisplaySettingsW(monInfo.szDevice, ModeNumber, &devMode))
      return false;

    convertMode(devMode, pMode);

    return true;
  }


  bool getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         ModeNumber,
          WsiMode*         pMode) {
    return retrieveDisplayMode(hMonitor, ModeNumber, pMode);
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