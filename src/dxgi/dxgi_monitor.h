#pragma once

#include <mutex>
#include <unordered_map>

#include "dxgi_interfaces.h"
#include "dxgi_options.h"

#include "../wsi/wsi_monitor.h"

namespace dxvk {

  class DxgiSwapChain;

  class DxgiMonitorInfo : public IDXGIVkMonitorInfo {

  public:

    DxgiMonitorInfo(IUnknown* pParent, const DxgiOptions& options);

    ~DxgiMonitorInfo();

    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);
    
    HRESULT STDMETHODCALLTYPE InitMonitorData(
            HMONITOR                hMonitor,
      const DXGI_VK_MONITOR_DATA*   pData);

    HRESULT STDMETHODCALLTYPE AcquireMonitorData(
            HMONITOR                hMonitor,
            DXGI_VK_MONITOR_DATA**  ppData);

    void STDMETHODCALLTYPE ReleaseMonitorData();

    void STDMETHODCALLTYPE PuntColorSpace(DXGI_COLOR_SPACE_TYPE ColorSpace);

    DXGI_COLOR_SPACE_TYPE STDMETHODCALLTYPE CurrentColorSpace() const;

    DXGI_COLOR_SPACE_TYPE DefaultColorSpace() const;

  private:

    IUnknown* m_parent;
    const DxgiOptions& m_options;

    dxvk::mutex                                        m_monitorMutex;
    std::unordered_map<HMONITOR, DXGI_VK_MONITOR_DATA> m_monitorData;

    std::atomic<DXGI_COLOR_SPACE_TYPE> m_globalColorSpace;

  };


  /**
   * \brief Queries bits per pixel for a format
   * 
   * The format must be a valid swap chain format.
   * \param [in] Format The DXGI format to query
   * \returns Bits per pixel for this format
   */
  uint32_t GetMonitorFormatBpp(
          DXGI_FORMAT             Format);

  /**
   * \brief Converts a DXVK WSI display mode to a DXGI display mode
   */
  inline DXGI_MODE_DESC1 ConvertDisplayMode(const wsi::WsiMode& WsiMode) {
    DXGI_MODE_DESC1 dxgiMode  = { };
    dxgiMode.Width            = WsiMode.width;
    dxgiMode.Height           = WsiMode.height;
    dxgiMode.RefreshRate      = DXGI_RATIONAL{ WsiMode.refreshRate.numerator, WsiMode.refreshRate.denominator };
    dxgiMode.Format           = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // FIXME
    dxgiMode.ScanlineOrdering = WsiMode.interlaced ? DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST : DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    dxgiMode.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    dxgiMode.Stereo           = FALSE;
    return dxgiMode;
  }

  /**
   * \brief Converts a DXGI display mode to a DXVK WSI display mode
   */
  inline wsi::WsiMode ConvertDisplayMode(const DXGI_MODE_DESC1& DxgiMode) {
    wsi::WsiMode wsiMode = { };
    wsiMode.width        = DxgiMode.Width;
    wsiMode.height       = DxgiMode.Height;
    wsiMode.refreshRate  = wsi::WsiRational{ DxgiMode.RefreshRate.Numerator, DxgiMode.RefreshRate.Denominator };
    wsiMode.bitsPerPixel = GetMonitorFormatBpp(DxgiMode.Format);
    wsiMode.interlaced   = DxgiMode.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST
                        || DxgiMode.ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
    return wsiMode;
  }

}