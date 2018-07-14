#include <algorithm>

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
    // Init output data if necessary
    if (FAILED(m_adapter->GetOutputData(m_monitor, nullptr))) {
      DXGI_VK_OUTPUT_DATA outputData;
      outputData.FrameStats = DXGI_FRAME_STATISTICS();
      outputData.GammaCurve.Scale  = { 1.0f, 1.0f, 1.0f };
      outputData.GammaCurve.Offset = { 0.0f, 0.0f, 0.0f };
      
      for (uint32_t i = 0; i < DXGI_VK_GAMMA_CP_COUNT; i++) {
        const float value = GammaControlPointLocation(i);
        outputData.GammaCurve.GammaCurve[i] = { value, value, value };
      }
      
      outputData.GammaDirty = FALSE;
      m_adapter->SetOutputData(m_monitor, &outputData);
    }
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
    if (pModeToMatch == nullptr || pClosestMatch == nullptr)
      return DXGI_ERROR_INVALID_CALL;

    if (pModeToMatch->Format == DXGI_FORMAT_UNKNOWN && pConcernedDevice == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    // If no format was specified, fall back to a standard
    // SRGB format, which is supported on all devices.
    DXGI_FORMAT targetFormat = pModeToMatch->Format;
    
    if (targetFormat == DXGI_FORMAT_UNKNOWN)
      targetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    
    UINT targetRefreshRate = 0;
    
    if (pModeToMatch->RefreshRate.Denominator != 0) {
      targetRefreshRate = pModeToMatch->RefreshRate.Numerator
                        / pModeToMatch->RefreshRate.Denominator;
    }
    
    // List all supported modes and filter
    // out those we don't actually need
    UINT modeCount = 0;
    GetDisplayModeList(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, nullptr);
    
    if (modeCount == 0) {
      Logger::err("DXGI: FindClosestMatchingMode: No modes found");
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

    // If no valid resolution is specified, find the
    // closest match for the current display resolution
    UINT targetWidth  = pModeToMatch->Width;
    UINT targetHeight = pModeToMatch->Height;

    if (targetWidth == 0 || targetHeight == 0) {
      DXGI_MODE_DESC activeMode = { };
      GetDisplayMode(&activeMode, ENUM_CURRENT_SETTINGS);

      targetWidth  = activeMode.Width;
      targetHeight = activeMode.Height;
    }

    // Select mode with minimal height+width difference
    UINT minDifference = std::numeric_limits<unsigned int>::max();
    
    for (auto mode : modes) {
      UINT currDifference = std::abs(int(targetWidth  - mode.Width))
                          + std::abs(int(targetHeight - mode.Height));

      if (currDifference <= minDifference) {
        minDifference = currDifference;
        *pClosestMatch = mode;
      }
    }

    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDesc(DXGI_OUTPUT_DESC *pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(m_monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DXGI: Failed to query monitor info");
      return E_FAIL;
    }
    
    std::memcpy(pDesc->DeviceName, monInfo.szDevice, std::size(pDesc->DeviceName));
    
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
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(m_monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DXGI: Failed to query monitor info");
      return E_FAIL;
    }
    
    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    DEVMODEW devMode;
    
    uint32_t srcModeId = 0;
    uint32_t dstModeId = 0;
    
    std::vector<DXGI_MODE_DESC> modeList;
    
    while (::EnumDisplaySettingsW(monInfo.szDevice, srcModeId++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.dmDisplayFlags & DM_INTERLACED)
        continue;
      
      // Skip modes with incompatible formats
      if (devMode.dmBitsPerPel != GetFormatBpp(EnumFormat))
        continue;
      
      if (pDesc != nullptr) {
        DXGI_MODE_DESC mode;
        mode.Width            = devMode.dmPelsWidth;
        mode.Height           = devMode.dmPelsHeight;
        mode.RefreshRate      = { devMode.dmDisplayFrequency * 1000, 1000 };
        mode.Format           = EnumFormat;
        mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        mode.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
        modeList.push_back(mode);
      }
      
      dstModeId += 1;
    }
    
    // Sort display modes by width, height and refresh rate,
    // in that order. Some games rely on correct ordering.
    std::sort(modeList.begin(), modeList.end(),
      [] (const DXGI_MODE_DESC& a, const DXGI_MODE_DESC& b) {
        if (a.Width < b.Width) return true;
        if (a.Width > b.Width) return false;
        
        if (a.Height < b.Height) return true;
        if (a.Height > b.Height) return false;
        
        return (a.RefreshRate.Numerator / a.RefreshRate.Denominator)
             < (b.RefreshRate.Numerator / b.RefreshRate.Denominator);
      });
    
    // If requested, write out the first set of display
    // modes to the destination array.
    if (pDesc != nullptr) {
      for (uint32_t i = 0; i < *pNumModes && i < dstModeId; i++)
        pDesc[i] = modeList[i];
      
      if (dstModeId > *pNumModes)
        return DXGI_ERROR_MORE_DATA;
    }
    
    *pNumModes = dstModeId;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplaySurfaceData(IDXGISurface* pDestination) {
    Logger::err("DxgiOutput::GetDisplaySurfaceData: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    DXGI_VK_OUTPUT_DATA outputData;
    
    if (FAILED(m_adapter->GetOutputData(m_monitor, &outputData))) {
      Logger::err("DXGI: Failed to query output data");
      return E_FAIL;
    }
    
    *pStats = outputData.FrameStats;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControl(DXGI_GAMMA_CONTROL* pArray) {
    DXGI_VK_OUTPUT_DATA outputData;
    
    if (FAILED(m_adapter->GetOutputData(m_monitor, &outputData))) {
      Logger::err("DXGI: Failed to query output data");
      return E_FAIL;
    }
    
    *pArray = outputData.GammaCurve;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps) {
    pGammaCaps->ScaleAndOffsetSupported = FALSE;
    pGammaCaps->MaxConvertedValue       = 1.0f;
    pGammaCaps->MinConvertedValue       = 0.0f;
    pGammaCaps->NumGammaControlPoints   = DXGI_VK_GAMMA_CP_COUNT;
    
    for (uint32_t i = 0; i < pGammaCaps->NumGammaControlPoints; i++)
      pGammaCaps->ControlPointPositions[i] = GammaControlPointLocation(i);
    return S_OK;
  }
  
  
  void STDMETHODCALLTYPE DxgiOutput::ReleaseOwnership() {
    Logger::warn("DxgiOutput::ReleaseOwnership: Stub");
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetDisplaySurface(IDXGISurface* pScanoutSurface) {
    Logger::err("DxgiOutput::SetDisplaySurface: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetGammaControl(const DXGI_GAMMA_CONTROL* pArray) {
    DXGI_VK_OUTPUT_DATA outputData;
    
    if (FAILED(m_adapter->GetOutputData(m_monitor, &outputData))) {
      Logger::err("DXGI: Failed to query output data");
      return E_FAIL;
    }
    
    outputData.GammaCurve = *pArray;
    outputData.GammaDirty = TRUE;
    
    if (FAILED(m_adapter->SetOutputData(m_monitor, &outputData))) {
      Logger::err("DXGI: Failed to update output data");
      return E_FAIL;
    } return S_OK;
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
  
  
  HRESULT DxgiOutput::GetDisplayMode(DXGI_MODE_DESC* pMode, DWORD ModeNum) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(m_monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
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
  
    
  HRESULT DxgiOutput::SetDisplayMode(const DXGI_MODE_DESC* pMode) {
    ::MONITORINFOEXW monInfo;
    monInfo.cbSize = sizeof(monInfo);

    if (!::GetMonitorInfoW(m_monitor, reinterpret_cast<MONITORINFO*>(&monInfo))) {
      Logger::err("DXGI: Failed to query monitor info");
      return E_FAIL;
    }
    
    DEVMODEW devMode = { };
    devMode.dmSize       = sizeof(devMode);
    devMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    devMode.dmPelsWidth  = pMode->Width;
    devMode.dmPelsHeight = pMode->Height;
    devMode.dmBitsPerPel = GetFormatBpp(pMode->Format);
    
    if (pMode->RefreshRate.Numerator != 0)  {
      devMode.dmFields |= DM_DISPLAYFREQUENCY;
      devMode.dmDisplayFrequency = pMode->RefreshRate.Numerator
                                 / pMode->RefreshRate.Denominator;
    }
    
    LONG status = ::ChangeDisplaySettingsExW(
      monInfo.szDevice, &devMode, nullptr, CDS_FULLSCREEN, nullptr);
    
    return status == DISP_CHANGE_SUCCESSFUL ? S_OK : DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;;
  }
  
  
  uint32_t DxgiOutput::GetFormatBpp(DXGI_FORMAT Format) const {
    DXGI_VK_FORMAT_INFO formatInfo = m_adapter->LookupFormat(Format, DXGI_VK_FORMAT_MODE_ANY);
    return imageFormatInfo(formatInfo.Format)->elementSize * 8;
  }
  
}
