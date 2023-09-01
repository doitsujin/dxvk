#include "../wsi_monitor.h"

#include "../../util/util_string.h"
#include "../../util/log/log.h"
#include "../../util/util_string.h"

#include <cstring>
#include <set>

#include <setupapi.h>
#include <ntddvdeo.h>
#include <cfgmgr32.h>

namespace dxvk::wsi {

  HMONITOR getDefaultMonitor() {
    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
  }

  struct MonitorEnumInfo {
    std::set<std::wstring> *gdiDeviceNames;
    UINT      iMonitorId;
    HMONITOR  oMonitor;
  };

  static BOOL CALLBACK MonitorEnumProc(
          HMONITOR                  hmon,
          HDC                       hdc,
          LPRECT                    rect,
          LPARAM                    lp) {
    auto data = reinterpret_cast<MonitorEnumInfo*>(lp);

    if (data->gdiDeviceNames)
    {
      MONITORINFOEXW monitorInfo;

      monitorInfo.cbSize = sizeof(monitorInfo);
      GetMonitorInfoW(hmon, (MONITORINFO *)&monitorInfo);
      if (data->gdiDeviceNames->find(monitorInfo.szDevice) == data->gdiDeviceNames->end())
        return TRUE;
    }
    if (data->iMonitorId--)
      return TRUE;
    data->oMonitor = hmon;
    return FALSE;
  }

  HMONITOR enumMonitors(uint32_t index) {
    MonitorEnumInfo info;
    info.iMonitorId = index;
    info.oMonitor   = nullptr;
    info.gdiDeviceNames = nullptr;

    ::EnumDisplayMonitors(
      nullptr, nullptr, &MonitorEnumProc,
      reinterpret_cast<LPARAM>(&info));

    return info.oMonitor;
  }

  HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index) {
    if (!numLUIDs)
      return enumMonitors(index);

    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    std::set<std::pair<uint32_t, uint32_t>> sources;
    std::set<std::wstring> gdiDeviceNames;
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    LONG result;

    do {
      if ((result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount))) {
        Logger::err(str::format("GetDisplayConfigBufferSizes failed, result ", result));
        return enumMonitors(index);
      }

      paths.resize(pathCount);
      modes.resize(modeCount);

      result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
        &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result) {
      Logger::err(str::format("QueryDisplayConfig failed, result ", result));
      return enumMonitors(index);
    }

    paths.resize(pathCount);
    modes.resize(modeCount);

    MonitorEnumInfo info;
    info.iMonitorId = index;
    info.oMonitor = nullptr;
    info.gdiDeviceNames = &gdiDeviceNames;

    for (const auto &path : paths) {
      uint32_t i;

      for (i = 0; i < numLUIDs; ++i) {
        if (!std::memcmp(&path.sourceInfo.adapterId, adapterLUID[i], sizeof(path.sourceInfo.adapterId)))
          break;
      }

      if (i == numLUIDs)
        continue;

      /* Mirrored displays appear as multiple paths with the same
       * GDI device name, that comes as single dxgi output. */
      if (!sources.insert(std::pair<uint32_t, uint32_t>(i, path.sourceInfo.id)).second)
        continue;

      DISPLAYCONFIG_SOURCE_DEVICE_NAME deviceName = { };
      deviceName.header.adapterId = path.sourceInfo.adapterId;
      deviceName.header.id = path.sourceInfo.id;
      deviceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
      deviceName.header.size = sizeof(deviceName);

      if ((result = DisplayConfigGetDeviceInfo(&deviceName.header))) {
        Logger::err(str::format("DisplayConfigGetDeviceInfo failed, result ", result));
        return enumMonitors(index);
      }

      gdiDeviceNames.insert(deviceName.viewGdiDeviceName);
    }
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

  static std::wstring getMonitorDevicePath(HMONITOR hMonitor) {
    // Get the device name of the monitor.
    MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);
    if (!::GetMonitorInfoW(hMonitor, &monInfo)) {
      Logger::err("getMonitorDevicePath: Failed to get monitor info.");
      return {};
    }

    // Try and find the monitor device path that matches
    // the name of our HMONITOR from the monitor info.
    LONG result = ERROR_SUCCESS;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    do {
      uint32_t pathCount = 0, modeCount = 0;
      if ((result = ::GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount)) != ERROR_SUCCESS) {
        Logger::err(str::format("getMonitorDevicePath: GetDisplayConfigBufferSizes failed. ret: ", result, " LastError: ", GetLastError()));
        return {};
      }
      paths.resize(pathCount);
      modes.resize(modeCount);
      result = ::QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result != ERROR_SUCCESS) {
      Logger::err(str::format("getMonitorDevicePath: QueryDisplayConfig failed. ret: ", result, " LastError: ", GetLastError()));
      return {};
    }

    // Link a source name -> target name
    for (const auto& path : paths) {
      DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
      sourceName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
      sourceName.header.size      = sizeof(sourceName);
      sourceName.header.adapterId = path.sourceInfo.adapterId;
      sourceName.header.id        = path.sourceInfo.id;
      if ((result = ::DisplayConfigGetDeviceInfo(&sourceName.header)) != ERROR_SUCCESS) {
        Logger::err(str::format("getMonitorDevicePath: DisplayConfigGetDeviceInfo with DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME failed. ret: ", result, " LastError: ", GetLastError()));
        continue;
      }

      DISPLAYCONFIG_TARGET_DEVICE_NAME targetName;
      targetName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
      targetName.header.size      = sizeof(targetName);
      targetName.header.adapterId = path.targetInfo.adapterId;
      targetName.header.id        = path.targetInfo.id;
      if ((result = ::DisplayConfigGetDeviceInfo(&targetName.header)) != ERROR_SUCCESS) {
        Logger::err(str::format("getMonitorDevicePath: DisplayConfigGetDeviceInfo with DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME failed. ret: ", result, " LastError: ", GetLastError()));
        continue;
      }

      // Does the source match the GDI device we are looking for?
      // If so, return the target back.
      if (!wcscmp(sourceName.viewGdiDeviceName, monInfo.szDevice))
        return targetName.monitorDevicePath;
    }

    Logger::err("getMonitorDevicePath: Failed to find a link from source -> target.");
    return {};
  }

  static WsiEdidData readMonitorEdidFromKey(HKEY deviceRegKey) {
    DWORD edidSize = 0;
    if (::RegQueryValueExW(deviceRegKey, L"EDID", nullptr, nullptr, nullptr, &edidSize) != ERROR_SUCCESS) {
      Logger::err("readMonitorEdidFromKey: Failed to get EDID reg key size");
      return {};
    }

    WsiEdidData edidData(edidSize);
    if (::RegQueryValueExW(deviceRegKey, L"EDID", nullptr, nullptr, edidData.data(), &edidSize) != ERROR_SUCCESS) {
      Logger::err("readMonitorEdidFromKey: Failed to get EDID reg key data");
      return {};
    }

    return edidData;
  }

  struct DxvkDeviceInterfaceDetail {
    // SP_DEVICE_INTERFACE_DETAIL_DATA_W contains an array called
    // "ANYSIZE_ARRAY" which is just 1 wchar_t in size.
    // Incredible, safe, and smart API design.
    // Allocate some chars after so it can fill these in.
    SP_DEVICE_INTERFACE_DETAIL_DATA_W base;
    wchar_t extraChars[MAX_DEVICE_ID_LEN];
  };

  WsiEdidData getMonitorEdid(HMONITOR hMonitor) {
    static constexpr GUID GUID_DEVINTERFACE_MONITOR = { 0xe6f07b5f, 0xee97, 0x4a90, 0xb0, 0x76, 0x33, 0xf5, 0x7b, 0xf4, 0xea, 0xa7 };
    static auto pfnSetupDiGetClassDevsW             = reinterpret_cast<decltype(SetupDiGetClassDevsW)*>            (::GetProcAddress(::GetModuleHandleW(L"setupapi.dll"), "SetupDiGetClassDevsW"));
    static auto pfnSetupDiEnumDeviceInterfaces      = reinterpret_cast<decltype(SetupDiEnumDeviceInterfaces)*>     (::GetProcAddress(::GetModuleHandleW(L"setupapi.dll"), "SetupDiEnumDeviceInterfaces"));
    static auto pfnSetupDiGetDeviceInterfaceDetailW = reinterpret_cast<decltype(SetupDiGetDeviceInterfaceDetailW)*>(::GetProcAddress(::GetModuleHandleW(L"setupapi.dll"), "SetupDiGetDeviceInterfaceDetailW"));
    static auto pfnSetupDiOpenDevRegKey             = reinterpret_cast<decltype(SetupDiOpenDevRegKey)*>            (::GetProcAddress(::GetModuleHandleW(L"setupapi.dll"), "SetupDiOpenDevRegKey"));
    static auto pfnSetupDiGetDeviceInstanceIdW      = reinterpret_cast<decltype(SetupDiGetDeviceInstanceIdW)*>     (::GetProcAddress(::GetModuleHandleW(L"setupapi.dll"), "SetupDiGetDeviceInstanceIdW"));
    if (!pfnSetupDiGetClassDevsW || !pfnSetupDiEnumDeviceInterfaces || !pfnSetupDiGetDeviceInterfaceDetailW || !pfnSetupDiOpenDevRegKey || !pfnSetupDiGetDeviceInstanceIdW) {
      Logger::err("getMonitorEdid: Failed to load functions from setupapi.");
      return {};
    }

    std::wstring monitorDevicePath = getMonitorDevicePath(hMonitor);
    if (monitorDevicePath.empty()) {
      Logger::err("getMonitorEdid: Failed to get monitor device path.");
      return {};
    }

    const HDEVINFO devInfo = pfnSetupDiGetClassDevsW(&GUID_DEVINTERFACE_MONITOR, nullptr, nullptr, DIGCF_DEVICEINTERFACE);

    SP_DEVICE_INTERFACE_DATA interfaceData;
    memset(&interfaceData, 0, sizeof(interfaceData));
    interfaceData.cbSize = sizeof(interfaceData);

    for (DWORD monitorIdx = 0; pfnSetupDiEnumDeviceInterfaces(devInfo, nullptr, &GUID_DEVINTERFACE_MONITOR, monitorIdx, &interfaceData); monitorIdx++) {
      DxvkDeviceInterfaceDetail detailData;
      // Josh: I'm taking no chances here. I don't trust this API at all.
      memset(&detailData, 0, sizeof(detailData));
      detailData.base.cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

      SP_DEVINFO_DATA devInfoData;
      memset(&devInfoData, 0, sizeof(devInfoData));
      devInfoData.cbSize = sizeof(devInfoData);

      if (!pfnSetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, &detailData.base, sizeof(detailData), nullptr, &devInfoData))
        continue;

      // Check that this monitor matches the same one we are looking for.
      // Note: For some reason the casing mismatches here, because this
      // is a well-designed API.
      // If it isn't what we are looking for, move along.
      if (_wcsicmp(monitorDevicePath.c_str(), detailData.base.DevicePath) != 0)
        continue;

      HKEY deviceRegKey = pfnSetupDiOpenDevRegKey(devInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
      if (deviceRegKey == INVALID_HANDLE_VALUE) {
        Logger::err("getMonitorEdid: Failed to open monitor device registry key.");
        return {};
      }

      auto edidData = readMonitorEdidFromKey(deviceRegKey);

      ::RegCloseKey(deviceRegKey);

      return edidData;
    }

    Logger::err("getMonitorEdid: Failed to find device interface for monitor using setupapi.");
    return {};
  }

}
