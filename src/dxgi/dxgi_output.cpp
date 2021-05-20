#include <algorithm>
#include <numeric>

#include <cstdlib>
#include <cstring>

#include <sstream>
#include <string>

#include "dxgi_adapter.h"
#include "dxgi_factory.h"
#include "dxgi_output.h"
#include "dxgi_swapchain.h"

#include "../dxvk/dxvk_format.h"

namespace dxvk {
  
  DxgiOutput::DxgiOutput(
    const Com<DxgiFactory>& factory,
    const Com<DxgiAdapter>& adapter,
              HMONITOR      monitor)
  : m_monitorInfo(factory->GetMonitorInfo()),
    m_adapter(adapter),
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
    
    m_monitorInfo->InitMonitorData(monitor, &monitorData);    
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
     || riid == __uuidof(IDXGIOutput4)
     || riid == __uuidof(IDXGIOutput5)
     || riid == __uuidof(IDXGIOutput6)) {
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

    // Both or neither must be zero
    if ((pModeToMatch->Width == 0) ^ (pModeToMatch->Height == 0))
      return DXGI_ERROR_INVALID_CALL;

    DEVMODEW devMode;
    devMode.dmSize = sizeof(devMode);

    if (!GetMonitorDisplayMode(m_monitor, ENUM_CURRENT_SETTINGS, &devMode))
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;

    DXGI_MODE_DESC activeMode = { };
    activeMode.Width            = devMode.dmPelsWidth;
    activeMode.Height           = devMode.dmPelsHeight;
    activeMode.RefreshRate      = { devMode.dmDisplayFrequency, 1 };
    activeMode.Format           = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // FIXME
    activeMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    activeMode.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_MODE_DESC1 defaultMode;
    defaultMode.Width            = 0;
    defaultMode.Height           = 0;
    defaultMode.RefreshRate      = { 0, 0 };
    defaultMode.Format           = DXGI_FORMAT_UNKNOWN;
    defaultMode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    defaultMode.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    defaultMode.Stereo           = pModeToMatch->Stereo;

    if (pModeToMatch->ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED)
      defaultMode.ScanlineOrdering = activeMode.ScanlineOrdering;

    if (pModeToMatch->Scaling == DXGI_MODE_SCALING_UNSPECIFIED)
      defaultMode.Scaling = activeMode.Scaling;

    DXGI_FORMAT targetFormat = pModeToMatch->Format;

    if (pModeToMatch->Format == DXGI_FORMAT_UNKNOWN) {
      defaultMode.Format = activeMode.Format;
      targetFormat       = activeMode.Format;
    }

    if (!pModeToMatch->Width) {
      defaultMode.Width  = activeMode.Width;
      defaultMode.Height = activeMode.Height;
    }

    if (!pModeToMatch->RefreshRate.Numerator || !pModeToMatch->RefreshRate.Denominator) {
      defaultMode.RefreshRate.Numerator   = activeMode.RefreshRate.Numerator;
      defaultMode.RefreshRate.Denominator = activeMode.RefreshRate.Denominator;
    }

    UINT modeCount = 0;
    GetDisplayModeList1(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, nullptr);
    
    if (modeCount == 0) {
      Logger::err("DXGI: FindClosestMatchingMode: No modes found");
      return DXGI_ERROR_NOT_FOUND;
    }

    std::vector<DXGI_MODE_DESC1> modes(modeCount);
    GetDisplayModeList1(targetFormat, DXGI_ENUM_MODES_SCALING, &modeCount, modes.data());

    FilterModesByDesc(modes, *pModeToMatch);
    FilterModesByDesc(modes, defaultMode);

    if (modes.empty())
      return DXGI_ERROR_NOT_FOUND;

    *pClosestMatch = modes[0];

    Logger::debug(str::format(
      "DXGI: For mode ",
        pModeToMatch->Width, "x", pModeToMatch->Height, "@",
        pModeToMatch->RefreshRate.Denominator ? (pModeToMatch->RefreshRate.Numerator / pModeToMatch->RefreshRate.Denominator) : 0,
      " found closest mode ",
        pClosestMatch->Width, "x", pClosestMatch->Height, "@",
        pClosestMatch->RefreshRate.Denominator ? (pClosestMatch->RefreshRate.Numerator / pClosestMatch->RefreshRate.Denominator) : 0));
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDesc(DXGI_OUTPUT_DESC *pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;

    DXGI_OUTPUT_DESC1 desc;
    HRESULT hr = GetDesc1(&desc);

    if (SUCCEEDED(hr)) {
      std::memcpy(pDesc->DeviceName, desc.DeviceName, sizeof(pDesc->DeviceName));
      pDesc->DesktopCoordinates = desc.DesktopCoordinates;
      pDesc->AttachedToDesktop  = desc.AttachedToDesktop;
      pDesc->Rotation           = desc.Rotation;
      pDesc->Monitor            = desc.Monitor;
    }

    return hr;
  }


  HRESULT STDMETHODCALLTYPE DxgiOutput::GetDesc1(
          DXGI_OUTPUT_DESC1*    pDesc) {
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
    pDesc->BitsPerColor       = 8;
    pDesc->ColorSpace         = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    // We don't really have a way to get these
    for (uint32_t i = 0; i < 2; i++) {
      pDesc->RedPrimary[i]    = 0.0f;
      pDesc->GreenPrimary[i]  = 0.0f;
      pDesc->BluePrimary[i]   = 0.0f;
      pDesc->WhitePoint[i]    = 0.0f;
    }

    pDesc->MinLuminance       = 0.0f;
    pDesc->MaxLuminance       = 0.0f;
    pDesc->MaxFullFrameLuminance = 0.0f;
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
      modes.resize(std::max(1u, *pNumModes));
    
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
    
    // Special case, just return zero modes
    if (EnumFormat == DXGI_FORMAT_UNKNOWN) {
      *pNumModes = 0;
      return S_OK;
    }

    // Walk over all modes that the display supports and
    // return those that match the requested format etc.
    DEVMODEW devMode = { };
    devMode.dmSize = sizeof(DEVMODEW);
    
    uint32_t srcModeId = 0;
    uint32_t dstModeId = 0;
    
    std::vector<DXGI_MODE_DESC1> modeList;
    
    while (GetMonitorDisplayMode(m_monitor, srcModeId++, &devMode)) {
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
    HRESULT hr = m_monitorInfo->AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;

    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("DxgiOutput::GetFrameStatistics: Stub");

    *pStats = monitorInfo->FrameStats;
    m_monitorInfo->ReleaseMonitorData();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiOutput::GetGammaControl(DXGI_GAMMA_CONTROL* pArray) {
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;
    HRESULT hr = m_monitorInfo->AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;
    
    *pArray = monitorInfo->GammaCurve;
    m_monitorInfo->ReleaseMonitorData();
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
    HRESULT hr = m_monitorInfo->AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;
    
    monitorInfo->GammaCurve = *pArray;

    if (monitorInfo->pSwapChain) {
      hr = monitorInfo->pSwapChain->SetGammaControl(
        DXGI_VK_GAMMA_CP_COUNT, pArray->GammaCurve);
    }

    m_monitorInfo->ReleaseMonitorData();
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
    return DuplicateOutput1(pDevice, 0, 0, nullptr, ppOutputDuplication);
  }


  HRESULT STDMETHODCALLTYPE DxgiOutput::DuplicateOutput1(
          IUnknown*                 pDevice,
          UINT                      Flags,
          UINT                      SupportedFormatsCount,
    const DXGI_FORMAT*              pSupportedFormats,
          IDXGIOutputDuplication**  ppOutputDuplication) {
    InitReturnPtr(ppOutputDuplication);

    if (!pDevice)
      return E_INVALIDARG;
    
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("DxgiOutput::DuplicateOutput1: Not implemented");
    
    // At least return a valid error code
    return DXGI_ERROR_UNSUPPORTED;
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
  

  HRESULT STDMETHODCALLTYPE DxgiOutput::CheckHardwareCompositionSupport(
          UINT*                 pFlags) {
    Logger::warn("DxgiOutput: CheckHardwareCompositionSupport: Stub");

    *pFlags = 0;
    return S_OK;
  }


  void DxgiOutput::FilterModesByDesc(
          std::vector<DXGI_MODE_DESC1>& Modes,
    const DXGI_MODE_DESC1&              TargetMode) {
    uint32_t minDiffResolution  = 0;
    uint32_t minDiffRefreshRate = 0;

    if (TargetMode.Width) {
      minDiffResolution = std::accumulate(
        Modes.begin(), Modes.end(), std::numeric_limits<uint32_t>::max(),
        [&TargetMode] (uint32_t current, const DXGI_MODE_DESC1& mode) {
          uint32_t diff = std::abs(int32_t(TargetMode.Width  - mode.Width))
                        + std::abs(int32_t(TargetMode.Height - mode.Height));
          return std::min(current, diff);
        });
    }

    if (TargetMode.RefreshRate.Numerator && TargetMode.RefreshRate.Denominator) {
      minDiffRefreshRate = std::accumulate(
        Modes.begin(), Modes.end(), std::numeric_limits<uint64_t>::max(),
        [&TargetMode] (uint64_t current, const DXGI_MODE_DESC1& mode) {
          uint64_t rate = uint64_t(mode.RefreshRate.Numerator)
                        * uint64_t(TargetMode.RefreshRate.Denominator)
                        / uint64_t(mode.RefreshRate.Denominator);
          uint64_t diff = std::abs(int64_t(rate - uint64_t(TargetMode.RefreshRate.Numerator)));
          return std::min(current, diff);
        });
    }

    bool testScanlineOrder = false;
    bool testScaling       = false;
    bool testFormat        = false;

    for (const auto& mode : Modes) {
      testScanlineOrder |= TargetMode.ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED
                        && TargetMode.ScanlineOrdering == mode.ScanlineOrdering;
      testScaling       |= TargetMode.Scaling != DXGI_MODE_SCALING_UNSPECIFIED
                        && TargetMode.Scaling == mode.Scaling;
      testFormat        |= TargetMode.Format != DXGI_FORMAT_UNKNOWN
                        && TargetMode.Format == mode.Format;
    }

    for (auto it = Modes.begin(); it != Modes.end(); ) {
      bool skipMode = it->Stereo != TargetMode.Stereo;

      if (testScanlineOrder)
        skipMode |= it->ScanlineOrdering != TargetMode.ScanlineOrdering;

      if (testScaling)
        skipMode |= it->Scaling != TargetMode.Scaling;

      if (testFormat)
        skipMode |= it->Format != TargetMode.Format;

      if (TargetMode.Width) {
        uint32_t diff = std::abs(int32_t(TargetMode.Width  - it->Width))
                      + std::abs(int32_t(TargetMode.Height - it->Height));
        skipMode |= diff != minDiffResolution;
      }

      if (TargetMode.RefreshRate.Numerator && TargetMode.RefreshRate.Denominator) {
        uint64_t rate = uint64_t(it->RefreshRate.Numerator)
                      * uint64_t(TargetMode.RefreshRate.Denominator)
                      / uint64_t(it->RefreshRate.Denominator);
        uint64_t diff = std::abs(int64_t(rate - uint64_t(TargetMode.RefreshRate.Numerator)));
        skipMode |= diff != minDiffRefreshRate;
      }

      it = skipMode ? Modes.erase(it) : ++it;
    }
  }

}
