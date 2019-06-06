#include "d3d9_monitor.h"

#include "d3d9_format.h"

namespace dxvk {

  uint32_t GetMonitorFormatBpp(D3D9Format Format) {
    switch (Format) {
    case D3D9Format::A8R8G8B8:
    case D3D9Format::X8R8G8B8: // This is still 32 bit even though the alpha is unspecified.
    case D3D9Format::A2R10G10B10:
      return 32;

    case D3D9Format::A1R5G5B5:
    case D3D9Format::X1R5G5B5:
    case D3D9Format::R5G6B5:
      return 16;

    default:
      Logger::warn(str::format(
        "GetMonitorFormatBpp: Unknown format: ",
        Format));
      return 32;
    }
  }


  bool IsSupportedBackBufferFormat(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed) {
    if (!IsSupportedMonitorFormat(AdapterFormat, Windowed))
      return false;

    bool similar = AdapterFormat == BackBufferFormat;

    similar |= AdapterFormat == D3D9Format::X8R8G8B8 && BackBufferFormat == D3D9Format::A8R8G8B8;
    similar |= AdapterFormat == D3D9Format::X1R5G5B5 && BackBufferFormat == D3D9Format::A1R5G5B5;

    return similar;
  }


  bool IsSupportedMonitorFormat(
          D3D9Format Format,
          BOOL       Windowed) {
    if (Format == D3D9Format::A2R10G10B10 && Windowed)
      return false;

    if (Format == D3D9Format::A2R10G10B10
     || Format == D3D9Format::X8R8G8B8
     || Format == D3D9Format::X1R5G5B5
     || Format == D3D9Format::R5G6B5)
      return true;

    return false;
  }


  bool IsSupportedBackBufferFormat(
          D3D9Format BackBufferFormat,
          BOOL       Windowed) {
    if (IsSupportedMonitorFormat(BackBufferFormat, Windowed))
      return true;

    if (BackBufferFormat == D3D9Format::A8R8G8B8
     || BackBufferFormat == D3D9Format::A1R5G5B5)
      return true;

    return false;
  }


  HMONITOR GetDefaultMonitor() {
    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
  }


  HRESULT SetMonitorDisplayMode(
          HMONITOR                hMonitor,
    const D3DDISPLAYMODEEX*       pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("D3D9: Failed to query monitor info");
      return E_FAIL;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    devMode.dmPelsWidth  = pMode->Width;
    devMode.dmPelsHeight = pMode->Height;
    devMode.dmBitsPerPel = GetMonitorFormatBpp(EnumerateFormat(pMode->Format));
    
    if (pMode->RefreshRate != 0)  {
      devMode.dmFields |= DM_DISPLAYFREQUENCY;
      devMode.dmDisplayFrequency = pMode->RefreshRate;
    }
    
    Logger::info(str::format("D3D9: Setting display mode: ",
      devMode.dmPelsWidth, "x", devMode.dmPelsHeight, "@",
      devMode.dmDisplayFrequency));
    
    LONG status = ::ChangeDisplaySettingsExW(
      monInfo.szDevice, &devMode, nullptr, CDS_FULLSCREEN, nullptr);
    
    return status == DISP_CHANGE_SUCCESSFUL ? D3D_OK : D3DERR_NOTAVAILABLE;
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
      Logger::err("D3D9: Failed to query monitor info");
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
      Logger::err("D3D9: Failed to query monitor info");
      return;
    }

    *pRect = monInfo.rcMonitor;
  }

}