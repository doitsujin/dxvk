#include <cstdlib>
#include <cstring>

#include <sstream>
#include <string>

#include "dxgi_adapter.h"
#include "dxgi_output.h"
#include "dxgi_swapchain.h"

#include "../dxvk/dxvk_format.h"

namespace dxvk {
  
  DxgiOutput::DxgiOutput(
    const Com<DxgiAdapter>& adapter,
              HMONITOR      monitor)
  : m_adapter(adapter),
    m_monitor(monitor) {
    
  }
  
  
  DxgiOutput::~DxgiOutput() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIOutput)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("DxgiOutput::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetParent(REFIID riid, void **ppParent) {
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
    
    // If no format was specified, fall back to a standard
    // SRGB format, which is supported on all devices.
    DXGI_FORMAT targetFormat = pModeToMatch->Format;
    
    if (targetFormat == DXGI_FORMAT_UNKNOWN)
      targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      
    UINT targetRefreshRate = 0;
    
    if (pModeToMatch->RefreshRate.Denominator != 0
     && pModeToMatch->RefreshRate.Numerator   != 0) {
      targetRefreshRate = pModeToMatch->RefreshRate.Numerator
                        / pModeToMatch->RefreshRate.Denominator;
    }
    
    // List all supported modes and filter
    // out those we don't actually need
    UINT modeCount = 0;
    GetDisplayModeList(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, nullptr);
    
    if (modeCount == 0) {
      Logger::err("DxgiOutput::FindClosestMatchingMode: No modes found");
      return DXGI_ERROR_NOT_FOUND;
    }

    std::vector<DXGI_MODE_DESC> modes(modeCount);
    GetDisplayModeList(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, modes.data());
    
    for (auto it = modes.begin(); it != modes.end(); ) {
      bool skipMode = false;
      
      // Remove modes with a different refresh rate
      if (targetRefreshRate != 0) {
        UINT modeRefreshRate = it->RefreshRate.Numerator
                             / it->RefreshRate.Denominator;
        skipMode |= modeRefreshRate != targetRefreshRate;
      }
      
      // Remove modes with incorrect scaling
      if (pModeToMatch->Scaling != DXGI_MODE_SCALING_UNSPECIFIED)
        skipMode |= it->Scaling != pModeToMatch->Scaling;
      
      it = skipMode ? modes.erase(it) : ++it;
    }
    
    // No matching modes found
    if (modes.size() == 0)
      return DXGI_ERROR_NOT_FOUND;

    // Select mode with minimal height+width difference
    UINT minDifference = std::numeric_limits<unsigned int>::max();
    
    for (auto& mode : modes) {
      UINT currDifference = std::abs(int(pModeToMatch->Width  - mode.Width))
                          + std::abs(int(pModeToMatch->Height - mode.Height));

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

    if (!::GetMonitorInfo(m_monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DxgiOutput: Failed to query monitor info");
      return E_FAIL;
    }
    
    std::memset(pDesc->DeviceName, 0, sizeof(pDesc->DeviceName));
    std::mbstowcs(pDesc->DeviceName, monInfo.szDevice, std::size(pDesc->DeviceName) - 1);
    
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

    if (!::GetMonitorInfo(m_monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DxgiOutput: Failed to query monitor info");
      return E_FAIL;
    }
    
    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    DEVMODE devMode;
    
    uint32_t srcModeId = 0;
    uint32_t dstModeId = 0;
    
    const bool includeStretchedModes = (Flags & DXGI_ENUM_MODES_SCALING);
    
    while (::EnumDisplaySettings(monInfo.szDevice, srcModeId++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.dmDisplayFlags & DM_INTERLACED)
        continue;
      
      // Skip modes with incompatible formats
      if (devMode.dmBitsPerPel != GetFormatBpp(EnumFormat))
        continue;
      
      // Skip stretched modes unless they are requested
      const bool isStretched = devMode.dmPelsWidth  != UINT(monInfo.rcMonitor.right  - monInfo.rcMonitor.left)
                            || devMode.dmPelsHeight != UINT(monInfo.rcMonitor.bottom - monInfo.rcMonitor.top);
      
      if (isStretched && !includeStretchedModes)
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
        mode.Scaling          = isStretched
          ? DXGI_MODE_SCALING_STRETCHED
          : DXGI_MODE_SCALING_CENTERED;
        
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
    pGammaCaps->ScaleAndOffsetSupported = TRUE;
    pGammaCaps->MaxConvertedValue       = 1.0f;
    pGammaCaps->MinConvertedValue       = 0.0f;
    pGammaCaps->NumGammaControlPoints   = DxgiPresenterGammaRamp::CpCount;
    
    for (uint32_t i = 0; i < pGammaCaps->NumGammaControlPoints; i++)
      pGammaCaps->ControlPointPositions[i] = DxgiPresenterGammaRamp::cpLocation(i);
    return S_OK;
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
