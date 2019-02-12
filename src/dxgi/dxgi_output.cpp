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
    // Init monitor info if necessary
    DXGI_VK_MONITOR_DATA monitorData;
    monitorData.pSwapChain = nullptr;
    monitorData.FrameStats = DXGI_FRAME_STATISTICS();
    monitorData.GammaCurve.Scale  = { 1.0f, 1.0f, 1.0f };
    monitorData.GammaCurve.Offset = { 0.0f, 0.0f, 0.0f };
    
    for (uint32_t i = 0; i < DXGI_VK_GAMMA_CP_COUNT; i++) {
      const float value = GammaControlPointLocation(i);
      monitorData.GammaCurve.GammaCurve[i] = { value, value, value };
    }
    
    InitMonitorData(monitor, &monitorData);    
  }
  
  
  DxgiOutput::~DxgiOutput() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIOutput)
     || riid == __uuidof(IDXGIOutput1)
     || riid == __uuidof(IDXGIOutput2)
     || riid == __uuidof(IDXGIOutput3)
     || riid == __uuidof(IDXGIOutput4)) {
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
    if (!pModeToMatch || !pClosestMatch)
      return DXGI_ERROR_INVALID_CALL;
    
    DXGI_MODE_DESC1 modeToMatch;
    modeToMatch.Width            = pModeToMatch->Width;
    modeToMatch.Height           = pModeToMatch->Height;
    modeToMatch.RefreshRate      = pModeToMatch->RefreshRate;
    modeToMatch.Format           = pModeToMatch->Format;
    modeToMatch.ScanlineOrdering = pModeToMatch->ScanlineOrdering;
    modeToMatch.Scaling          = pModeToMatch->Scaling;
    modeToMatch.Stereo           = FALSE;

    DXGI_MODE_DESC1 closestMatch = { };

    HRESULT hr = FindClosestMatchingMode1(
      &modeToMatch, &closestMatch, pConcernedDevice);
    
    if (FAILED(hr))
      return hr;
    
    pClosestMatch->Width            = closestMatch.Width;
    pClosestMatch->Height           = closestMatch.Height;
    pClosestMatch->RefreshRate      = closestMatch.RefreshRate;
    pClosestMatch->Format           = closestMatch.Format;
    pClosestMatch->ScanlineOrdering = closestMatch.ScanlineOrdering;
    pClosestMatch->Scaling          = closestMatch.Scaling;
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::FindClosestMatchingMode1(
    const DXGI_MODE_DESC1*      pModeToMatch,
          DXGI_MODE_DESC1*      pClosestMatch,
          IUnknown*             pConcernedDevice) {
    if (!pModeToMatch || !pClosestMatch)
      return DXGI_ERROR_INVALID_CALL;

    if (pModeToMatch->Format == DXGI_FORMAT_UNKNOWN && !pConcernedDevice)
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
    GetDisplayModeList1(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, nullptr);
    
    if (modeCount == 0) {
      Logger::err("DXGI: FindClosestMatchingMode: No modes found");
      return DXGI_ERROR_NOT_FOUND;
    }

    std::vector<DXGI_MODE_DESC1> modes(modeCount);
    GetDisplayModeList1(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, modes.data());
    
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
      
      // Remove modes with incorrect stereo mode
      skipMode |= it->Stereo != pModeToMatch->Stereo;
      
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
      GetMonitorDisplayMode(m_monitor,
        ENUM_CURRENT_SETTINGS, &activeMode);

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
          UINT*          pNumModes,
          DXGI_MODE_DESC* pDesc) {
    if (pNumModes == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    std::vector<DXGI_MODE_DESC1> modes;

    if (pDesc)
      modes.resize(*pNumModes);
    
    HRESULT hr = GetDisplayModeList1(
      EnumFormat, Flags, pNumModes,
      pDesc ? modes.data() : nullptr);
    
    for (uint32_t i = 0; i < *pNumModes && i < modes.size(); i++) {
      pDesc[i].Width            = modes[i].Width;
      pDesc[i].Height           = modes[i].Height;
      pDesc[i].RefreshRate      = modes[i].RefreshRate;
      pDesc[i].Format           = modes[i].Format;
      pDesc[i].ScanlineOrdering = modes[i].ScanlineOrdering;
      pDesc[i].Scaling          = modes[i].Scaling;
    }
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplayModeList1(
          DXGI_FORMAT           EnumFormat,
          UINT                  Flags,
          UINT*                 pNumModes,
          DXGI_MODE_DESC1*      pDesc) {
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
    
    std::vector<DXGI_MODE_DESC1> modeList;
    
    while (::EnumDisplaySettingsW(monInfo.szDevice, srcModeId++, &devMode)) {
      // Skip interlaced modes altogether
      if (devMode.dmDisplayFlags & DM_INTERLACED)
        continue;
      
      // Skip modes with incompatible formats
      if (devMode.dmBitsPerPel != GetMonitorFormatBpp(EnumFormat))
        continue;
      
      if (pDesc != nullptr) {
        DXGI_MODE_DESC1 mode;
        mode.Width            = devMode.dmPelsWidth;
        mode.Height           = devMode.dmPelsHeight;
        mode.RefreshRate      = { devMode.dmDisplayFrequency * 1000, 1000 };
        mode.Format           = EnumFormat;
        mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        mode.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
        mode.Stereo           = FALSE;
        modeList.push_back(mode);
      }
      
      dstModeId += 1;
    }
    
    // Sort display modes by width, height and refresh rate,
    // in that order. Some games rely on correct ordering.
    std::sort(modeList.begin(), modeList.end(),
      [] (const DXGI_MODE_DESC1& a, const DXGI_MODE_DESC1& b) {
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
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;
    HRESULT hr = AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;
    
    *pStats = monitorInfo->FrameStats;
    ReleaseMonitorData();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControl(DXGI_GAMMA_CONTROL* pArray) {
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;
    HRESULT hr = AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;
    
    *pArray = monitorInfo->GammaCurve;
    ReleaseMonitorData();
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


  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDisplaySurfaceData1(IDXGIResource* pDestination) {
    Logger::err("DxgiOutput::SetDisplaySurface1: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::SetGammaControl(const DXGI_GAMMA_CONTROL* pArray) {
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;
    HRESULT hr = AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;
    
    monitorInfo->GammaCurve = *pArray;

    if (monitorInfo->pSwapChain) {
      hr = monitorInfo->pSwapChain->SetGammaControl(
        DXGI_VK_GAMMA_CP_COUNT, pArray->GammaCurve);
    }

    ReleaseMonitorData();
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::TakeOwnership(
          IUnknown *pDevice,
          BOOL     Exclusive) {
    Logger::warn("DxgiOutput::TakeOwnership: Stub");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::WaitForVBlank() {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("DxgiOutput::WaitForVBlank: Stub");
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::DuplicateOutput(
          IUnknown*                 pDevice,
          IDXGIOutputDuplication**  ppOutputDuplication) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("DxgiOutput::DuplicateOutput: Stub");
    
    return E_NOTIMPL;
  }


  BOOL DxgiOutput::SupportsOverlays() {
    return FALSE;
  }


  HRESULT STDMETHODCALLTYPE DxgiOutput::CheckOverlaySupport(
          DXGI_FORMAT EnumFormat,
          IUnknown*   pConcernedDevice,
          UINT*       pFlags) {
    Logger::warn("DxgiOutput: CheckOverlaySupport: Stub");
    return DXGI_ERROR_UNSUPPORTED;
  }


  HRESULT STDMETHODCALLTYPE DxgiOutput::CheckOverlayColorSpaceSupport(
          DXGI_FORMAT           Format,
          DXGI_COLOR_SPACE_TYPE ColorSpace,
          IUnknown*             pConcernedDevice,
          UINT*                 pFlags) {
    Logger::warn("DxgiOutput: CheckOverlayColorSpaceSupport: Stub");
    return DXGI_ERROR_UNSUPPORTED;
  }
  
}
