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

#include "../util/util_misc.h"
#include "../util/util_sleep.h"
#include "../util/util_time.h"

namespace dxvk {

  DxgiOutput::DxgiOutput(
    const Com<DxgiFactory>& factory,
    const Com<DxgiAdapter>& adapter,
              HMONITOR      monitor)
  : m_monitorInfo(factory->GetMonitorInfo()),
    m_adapter(adapter),
    m_monitor(monitor) {
    CacheMonitorData();
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
    
    if (logQueryInterfaceError(__uuidof(IDXGIOutput), riid)) {
      Logger::warn("DxgiOutput::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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

    wsi::WsiMode activeWsiMode = { };
    wsi::getCurrentDisplayMode(m_monitor, &activeWsiMode);

    DXGI_MODE_DESC1 activeMode = ConvertDisplayMode(activeWsiMode);

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
    
    if (!wsi::getDesktopCoordinates(m_monitor, &pDesc->DesktopCoordinates)) {
      Logger::err("DXGI: Failed to query monitor coords");
      return E_FAIL;
    }
    
    if (!wsi::getDisplayName(m_monitor, pDesc->DeviceName)) {
      Logger::err("DXGI: Failed to query monitor name");
      return E_FAIL;
    }

    pDesc->AttachedToDesktop     = 1;
    pDesc->Rotation              = DXGI_MODE_ROTATION_UNSPECIFIED;
    pDesc->Monitor               = m_monitor;
    pDesc->BitsPerColor          = 10;
    // This should only return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
    // (HDR) if the user has the HDR setting enabled in Windows.
    // Games can still punt into HDR mode by using CheckColorSpaceSupport
    // and SetColorSpace1.
    //
    // We have no way of checking the actual Windows colorspace as the
    // only public method for this *is* DXGI which we are re-implementing.
    // So we just pick our color space based on the DXVK_HDR env var
    // and the punting from SetColorSpace1.
    pDesc->ColorSpace            = m_monitorInfo->CurrentColorSpace();
    pDesc->RedPrimary[0]         = m_metadata.redPrimary[0];
    pDesc->RedPrimary[1]         = m_metadata.redPrimary[1];
    pDesc->GreenPrimary[0]       = m_metadata.greenPrimary[0];
    pDesc->GreenPrimary[1]       = m_metadata.greenPrimary[1];
    pDesc->BluePrimary[0]        = m_metadata.bluePrimary[0];
    pDesc->BluePrimary[1]        = m_metadata.bluePrimary[1];
    pDesc->WhitePoint[0]         = m_metadata.whitePoint[0];
    pDesc->WhitePoint[1]         = m_metadata.whitePoint[1];
    pDesc->MinLuminance          = m_metadata.minLuminance;
    pDesc->MaxLuminance          = m_metadata.maxLuminance;
    pDesc->MaxFullFrameLuminance = m_metadata.maxFullFrameLuminance;
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
    wsi::WsiMode devMode = { };
    
    uint32_t srcModeId = 0;
    uint32_t dstModeId = 0;
    
    std::vector<DXGI_MODE_DESC1> modeList;
    
    while (wsi::getDisplayMode(m_monitor, srcModeId++, &devMode)) {
      // Only enumerate interlaced modes if requested.
      if (devMode.interlaced && !(Flags & DXGI_ENUM_MODES_INTERLACED))
        continue;
      
      // Skip modes with incompatible formats
      if (devMode.bitsPerPixel != GetMonitorFormatBpp(EnumFormat))
        continue;
      
      if (pDesc != nullptr) {
        DXGI_MODE_DESC1 mode = ConvertDisplayMode(devMode);
        // Fix up the DXGI_FORMAT to match what we were enumerating.
        mode.Format = EnumFormat;

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

    // Need to acquire swap chain and unlock monitor data, since querying
    // frame statistics from the swap chain will also access monitor data.
    Com<IDXGISwapChain> swapChain = monitorInfo->pSwapChain;
    m_monitorInfo->ReleaseMonitorData();

    // This API only works if there is a full-screen swap chain active.
    if (swapChain == nullptr) {
      *pStats = DXGI_FRAME_STATISTICS();
      return S_OK;
    }

    return swapChain->GetFrameStatistics(pStats);
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
      Logger::warn("DxgiOutput::WaitForVBlank: Inaccurate");

    // Get monitor data to compute the sleep duration
    DXGI_VK_MONITOR_DATA* monitorInfo = nullptr;
    HRESULT hr = m_monitorInfo->AcquireMonitorData(m_monitor, &monitorInfo);

    if (FAILED(hr))
      return hr;

    // Estimate number of vblanks since last mode
    // change, then wait for one more refresh period
    auto refreshPeriod = computeRefreshPeriod(
      monitorInfo->LastMode.RefreshRate.Numerator,
      monitorInfo->LastMode.RefreshRate.Denominator);

    auto t0 = dxvk::high_resolution_clock::get_time_from_counter(monitorInfo->FrameStats.SyncQPCTime.QuadPart);
    auto t1 = dxvk::high_resolution_clock::now();

    uint64_t vblankCount = computeRefreshCount(t0, t1, refreshPeriod);
    auto t2 = t0 + (vblankCount + 1) * refreshPeriod;

    m_monitorInfo->ReleaseMonitorData();

    // Sleep until the given time point
    Sleep::sleepUntil(t1, t2);
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
    // Filter modes based on format properties
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

      it = skipMode ? Modes.erase(it) : ++it;
    }

    // Filter by closest resolution
    uint32_t minDiffResolution  = 0;

    if (TargetMode.Width) {
      minDiffResolution = std::accumulate(
        Modes.begin(), Modes.end(), std::numeric_limits<uint32_t>::max(),
        [&TargetMode] (uint32_t current, const DXGI_MODE_DESC1& mode) {
          uint32_t diff = std::abs(int32_t(TargetMode.Width  - mode.Width))
                        + std::abs(int32_t(TargetMode.Height - mode.Height));
          return std::min(current, diff);
        });

      for (auto it = Modes.begin(); it != Modes.end(); ) {
        uint32_t diff = std::abs(int32_t(TargetMode.Width  - it->Width))
                      + std::abs(int32_t(TargetMode.Height - it->Height));

        bool skipMode = diff != minDiffResolution;
        it = skipMode ? Modes.erase(it) : ++it;
      }
    }

    // Filter by closest refresh rate
    uint32_t minDiffRefreshRate = 0;

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

      for (auto it = Modes.begin(); it != Modes.end(); ) {
        uint64_t rate = uint64_t(it->RefreshRate.Numerator)
                      * uint64_t(TargetMode.RefreshRate.Denominator)
                      / uint64_t(it->RefreshRate.Denominator);
        uint64_t diff = std::abs(int64_t(rate - uint64_t(TargetMode.RefreshRate.Numerator)));

        bool skipMode = diff != minDiffRefreshRate;
        it = skipMode ? Modes.erase(it) : ++it;
      }
    }
  }


  void DxgiOutput::CacheMonitorData() {
    // Try and find an existing monitor info.
    DXGI_VK_MONITOR_DATA* pMonitorData;
    if (SUCCEEDED(m_monitorInfo->AcquireMonitorData(m_monitor, &pMonitorData))) {
      m_metadata = pMonitorData->DisplayMetadata;
      m_monitorInfo->ReleaseMonitorData();
      return;
    }

    // Init monitor info ourselves.
    // 
    // If some other thread ends up beating us to it
    // by another InitMonitorData, it doesn't really matter.
    // 
    // The only thing we cache from this is the m_metadata which
    // should be exactly the same.
    // We don't store any pointers from the DXGI_VK_MONITOR_DATA
    // sturcture, etc.
    DXGI_VK_MONITOR_DATA monitorData = {};

    // Query current display mode
    wsi::WsiMode activeWsiMode = { };
    wsi::getCurrentDisplayMode(m_monitor, &activeWsiMode);

    // Get the display metadata + colorimetry
    wsi::WsiEdidData edidData = wsi::getMonitorEdid(m_monitor);
    std::optional<wsi::WsiDisplayMetadata> metadata = std::nullopt;
    if (!edidData.empty())
      metadata = wsi::parseColorimetryInfo(edidData);

    if (metadata)
      m_metadata = metadata.value();
    else
      Logger::err("DXGI: Failed to parse display metadata + colorimetry info, using blank.");

    // Normalize either the display metadata we got back, or our
    // blank one to get something sane here.
    NormalizeDisplayMetadata(m_monitorInfo->DefaultColorSpace() != DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, m_metadata);

    auto refreshPeriod = computeRefreshPeriod(
      activeWsiMode.refreshRate.numerator,
      activeWsiMode.refreshRate.denominator);

    monitorData.FrameStats.SyncQPCTime.QuadPart = dxvk::high_resolution_clock::get_counter();
    monitorData.FrameStats.SyncRefreshCount = computeRefreshCount(
      dxvk::high_resolution_clock::time_point(),
      dxvk::high_resolution_clock::get_time_from_counter(monitorData.FrameStats.SyncQPCTime.QuadPart),
      refreshPeriod);

    monitorData.FrameStats.PresentRefreshCount = monitorData.FrameStats.SyncRefreshCount;
    monitorData.GammaCurve.Scale = { 1.0f, 1.0f, 1.0f };
    monitorData.GammaCurve.Offset = { 0.0f, 0.0f, 0.0f };
    monitorData.LastMode = ConvertDisplayMode(activeWsiMode);
    monitorData.DisplayMetadata = m_metadata;

    for (uint32_t i = 0; i < DXGI_VK_GAMMA_CP_COUNT; i++) {
      const float value = GammaControlPointLocation(i);
      monitorData.GammaCurve.GammaCurve[i] = { value, value, value };
    }

    m_monitorInfo->InitMonitorData(m_monitor, &monitorData);
  }

}
