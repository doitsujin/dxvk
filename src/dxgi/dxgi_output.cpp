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
    Logger::warn(str::format(riid));
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
    if (pModeToMatch == nullptr) {
      Logger::err("DxgiOutput::FindClosestMatchingMode: pModeToMatch is nullptr");
      return DXGI_ERROR_INVALID_CALL;
    }
        
    if (pClosestMatch == nullptr) {
      Logger::err("DxgiOutput::FindClosestMatchingMode: pClosestMatch is nullptr");
      return DXGI_ERROR_INVALID_CALL;
    }

    if (pModeToMatch->Format == DXGI_FORMAT_UNKNOWN && pConcernedDevice == nullptr) {
      Logger::err("DxgiOutput::FindClosestMatchingMode: no pointer to device was provided for DXGI_FORMAT_UNKNOWN format");
      return DXGI_ERROR_INVALID_CALL;
    }

    if (pModeToMatch->Format == DXGI_FORMAT_UNKNOWN) {
      /* TODO: perform additional format matching
      https://msdn.microsoft.com/en-us/library/windows/desktop/bb174547(v=vs.85).aspx?f=255&MSPPError=-2147217396
      > If pConcernedDevice is NULL, Format CANNOT be DXGI_FORMAT_UNKNOWN.
      and vice versa
      >If pConcernedDevice is NOT NULL, Format COULD be DXGI_FORMAT_UNKNOWN.
 
      But Format in structures from GetDisplayModeList() cannot be
      DXGI_FORMAT_UNKNOWN by definition.
      
      There is way in case of DXGI_FORMAT_UNKNOWN and pDevice != nullptr we
      should perform additional format matching. It may be just ignoring of
      Format field or using some range of formats but MSDN nothing says 
      about of criteria.  
      */
      Logger::err("DxgiOutput::FindClosestMatchingMode: matching formats to device currently is not supported");
      return DXGI_ERROR_UNSUPPORTED;
    }
 
    DXGI_MODE_DESC modeToMatch = *pModeToMatch;
    UINT modesCount = 0;
    GetDisplayModeList(pModeToMatch->Format, 0, &modesCount, nullptr);

    if (modesCount == 0) {
      Logger::err("DxgiOutput::FindClosestMatchingMode: no device formats were found");
      return DXGI_ERROR_NOT_FOUND;
    }

    std::vector<DXGI_MODE_DESC> modes(modesCount);
    GetDisplayModeList(pModeToMatch->Format, 0, &modesCount, modes.data());

    /* TODO: add scaling and scanline filter when we implement they */

    //filter out modes with different refresh rate if it was set
    if (modeToMatch.RefreshRate.Denominator != 0 && modeToMatch.RefreshRate.Numerator != 0) {
      UINT targetRefreshRate = modeToMatch.RefreshRate.Numerator / modeToMatch.RefreshRate.Denominator;
      for (auto it = modes.begin(); it != modes.end();) {
        UINT modeRefreshRate = it->RefreshRate.Numerator / it->RefreshRate.Denominator;
        if (modeRefreshRate != targetRefreshRate)
          it = modes.erase(it);
        else
          ++it;
      }
    }

    // return error when there is no modes with target refresh rate and scanline order
    if(modes.size() == 0) {
      Logger::err("DxgiOutput::FindClosestMatchingMode: no matched formats were found");
      return DXGI_ERROR_NOT_FOUND;
    }

    //select mode with minimal height+width difference
    UINT minDifference = UINT_MAX;
    for (auto& mode : modes) {
      UINT currDifference = abs((int)(modeToMatch.Width - mode.Width))
        + abs((int)(modeToMatch.Height - mode.Height));

      if (currDifference < minDifference) {
        minDifference = currDifference;
        *pClosestMatch = mode;
      }
    }

    return S_OK;
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

    if (Flags != 0)
      Logger::warn("DxgiOutput::GetDisplayModeList: flags are ignored");
    
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
