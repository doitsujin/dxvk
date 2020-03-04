#include "dxgi_monitor.h"

namespace dxvk {

  DxgiMonitorInfo::DxgiMonitorInfo(IUnknown* pParent)
  : m_parent(pParent) {

  }


  DxgiMonitorInfo::~DxgiMonitorInfo() {

  }


  ULONG STDMETHODCALLTYPE DxgiMonitorInfo::AddRef() {
    return m_parent->AddRef();
  }

  
  ULONG STDMETHODCALLTYPE DxgiMonitorInfo::Release() {
    return m_parent->Release();
  }
  

  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_parent->QueryInterface(riid, ppvObject);
  }
  

  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::InitMonitorData(
          HMONITOR                hMonitor,
    const DXGI_VK_MONITOR_DATA*   pData) {
    if (!hMonitor || !pData)
      return E_INVALIDARG;
    
    std::lock_guard<std::mutex> lock(m_monitorMutex);
    auto result = m_monitorData.insert({ hMonitor, *pData });

    return result.second ? S_OK : E_INVALIDARG;
  }


  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) {
    InitReturnPtr(ppData);

    if (!hMonitor || !ppData)
      return E_INVALIDARG;
    
    m_monitorMutex.lock();

    auto entry = m_monitorData.find(hMonitor);
    if (entry == m_monitorData.end()) {
      m_monitorMutex.unlock();
      return DXGI_ERROR_NOT_FOUND;
    }

    *ppData = &entry->second;
    return S_OK;
  }


  void STDMETHODCALLTYPE DxgiMonitorInfo::ReleaseMonitorData() {
    m_monitorMutex.unlock();
  }


  uint32_t GetMonitorFormatBpp(DXGI_FORMAT Format) {
    switch (Format) {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        return 32;
      
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return 64;
      
      default:
        Logger::warn(str::format(
          "GetMonitorFormatBpp: Unknown format: ",
          Format));
        return 32;
    }
  }


  HRESULT GetMonitorDisplayMode(
          HMONITOR                hMonitor,
          DWORD                   ModeNum,
          DXGI_MODE_DESC*         pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DXGI: Failed to query monitor info");
      return E_FAIL;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize = sizeof(devMode);
    
    if (!::EnumDisplaySettingsW(monInfo.szDevice, ModeNum, &devMode))
      return DXGI_ERROR_NOT_FOUND;
    
    pMode->Width            = devMode.dmPelsWidth;
    pMode->Height           = devMode.dmPelsHeight;
    pMode->RefreshRate      = { devMode.dmDisplayFrequency, 1 };
    pMode->Format           = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // FIXME
    pMode->ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    pMode->Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    return S_OK;
  }
  
    
  HRESULT SetMonitorDisplayMode(
          HMONITOR                hMonitor,
    const DXGI_MODE_DESC*         pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(hMonitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DXGI: Failed to query monitor info");
      return E_FAIL;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    devMode.dmPelsWidth  = pMode->Width;
    devMode.dmPelsHeight = pMode->Height;
    devMode.dmBitsPerPel = GetMonitorFormatBpp(pMode->Format);
    
    if (pMode->RefreshRate.Numerator != 0)  {
      devMode.dmFields |= DM_DISPLAYFREQUENCY;
      devMode.dmDisplayFrequency = pMode->RefreshRate.Numerator
                                 / pMode->RefreshRate.Denominator;
    }
    
    Logger::info(str::format("DXGI: Setting display mode: ",
      devMode.dmPelsWidth, "x", devMode.dmPelsHeight, "@",
      devMode.dmDisplayFrequency));
    
    LONG status = ::ChangeDisplaySettingsExW(
      monInfo.szDevice, &devMode, nullptr, CDS_FULLSCREEN, nullptr);
    
    return status == DISP_CHANGE_SUCCESSFUL ? S_OK : DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;;
  }
  
}