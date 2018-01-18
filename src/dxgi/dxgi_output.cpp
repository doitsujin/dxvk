#include <cstdlib>
#include <cstring>

#include <sstream>
#include <string>

#include "dxgi_adapter.h"
#include "dxgi_output.h"

#include "../dxvk/dxvk_format.h"

namespace dxvk {
  
  DxgiOutput::DxgiOutput(
              DxgiAdapter*  adapter,
              HMONITOR      monitor)
  : m_adapter(adapter),
    m_monitor(monitor) {
    
  }
  
  
  DxgiOutput::~DxgiOutput() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIOutput);
    
    Logger::warn("DxgiOutput::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetParent(
          REFIID riid,
          void   **ppParent) {
    return m_adapter->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::FindClosestMatchingMode(
    const DXGI_MODE_DESC *pModeToMatch,
          DXGI_MODE_DESC *pClosestMatch,
          IUnknown       *pConcernedDevice) {
    Logger::err("DxgiOutput::FindClosestMatchingMode: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDesc(DXGI_OUTPUT_DESC *pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    ::MONITORINFOEX monInfo;

    monInfo.cbSize = sizeof(monInfo);
    if (!::GetMonitorInfo(m_monitor, &monInfo)) {
      Logger::err("DxgiOutput: Failed to query monitor info");
      return E_FAIL;
    }
    
    std::memset(pDesc->DeviceName, 0, sizeof(pDesc->DeviceName));
    std::mbstowcs(pDesc->DeviceName, monInfo.szDevice, _countof(pDesc->DeviceName) - 1);
    
    pDesc->DesktopCoordinates = monInfo.rcMonitor;
    pDesc->AttachedToDesktop  = 1;
    pDesc->Rotation           = DXGI_MODE_ROTATION_UNSPECIFIED;
    pDesc->Monitor            = m_monitor;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplayModeList(
          DXGI_FORMAT    EnumFormat,
          UINT           Flags,
          UINT           *pNumModes,
          DXGI_MODE_DESC *pDesc) {
    if (pNumModes == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // Query monitor info to get the device name
    ::MONITORINFOEX monInfo;

    monInfo.cbSize = sizeof(monInfo);
    if (!::GetMonitorInfo(m_monitor, &monInfo)) {
      Logger::err("DxgiOutput: Failed to query monitor info");
      return E_FAIL;
    }
    
    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    DEVMODE devMode;
    
    uint32_t srcModeId = 0;
    uint32_t dstModeId = 0;
    
    while (::EnumDisplaySettings(monInfo.szDevice, srcModeId++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.dmDisplayFlags & DM_INTERLACED)
        continue;
      
      // Skip modes with incompatible formats
      if (devMode.dmBitsPerPel != GetFormatBpp(EnumFormat))
        continue;
      
      // Write back display mode
      if (pDesc != nullptr) {
        if (dstModeId >= *pNumModes)
          return DXGI_ERROR_MORE_DATA;
        
        DXGI_MODE_DESC mode;
        mode.Width            = devMode.dmPelsWidth;
        mode.Height           = devMode.dmPelsHeight;
        mode.RefreshRate      = { devMode.dmDisplayFrequency, 1 };
        mode.Format           = EnumFormat;
        mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        mode.Scaling          = DXGI_MODE_SCALING_CENTERED;
        pDesc[dstModeId] = mode;
      }
      
      dstModeId += 1;
    }
    
    *pNumModes = dstModeId;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplaySurfaceData(IDXGISurface *pDestination) {
    Logger::err("DxgiOutput::GetDisplaySurfaceData: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) {
    Logger::err("DxgiOutput::GetFrameStatistics: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControl(DXGI_GAMMA_CONTROL *pArray) {
    Logger::err("DxgiOutput::GetGammaControl: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps) {
    Logger::err("DxgiOutput::GetGammaControlCapabilities: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void STDMETHODCALLTYPE DxgiOutput::ReleaseOwnership() {
    Logger::warn("DxgiOutput::ReleaseOwnership: Stub");
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetDisplaySurface(IDXGISurface *pScanoutSurface) {
    Logger::err("DxgiOutput::SetDisplaySurface: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetGammaControl(const DXGI_GAMMA_CONTROL *pArray) {
    Logger::err("DxgiOutput::SetGammaControl: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::TakeOwnership(
          IUnknown *pDevice,
          BOOL     Exclusive) {
    Logger::warn("DxgiOutput::TakeOwnership: Stub");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::WaitForVBlank() {
    Logger::warn("DxgiOutput::WaitForVBlank: Stub");
    return S_OK;
  }
  
  
  uint32_t DxgiOutput::GetFormatBpp(DXGI_FORMAT Format) const {
    DxgiFormatInfo formatInfo = m_adapter->LookupFormat(Format, DxgiFormatMode::Any);
    return imageFormatInfo(formatInfo.format)->elementSize * 8;
  }
  
}
