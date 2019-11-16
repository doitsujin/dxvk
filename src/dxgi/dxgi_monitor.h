#pragma once

#include <mutex>
#include <unordered_map>

#include "dxgi_interfaces.h"

#include "../wsi/wsi_mode.h"

namespace dxvk {

  class DxgiSwapChain;

  class DxgiMonitorInfo : public IDXGIVkMonitorInfo {

  public:

    DxgiMonitorInfo(IUnknown* pParent);

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

  private:

    IUnknown* m_parent;

    std::mutex                                         m_monitorMutex;
    std::unordered_map<HMONITOR, DXGI_VK_MONITOR_DATA> m_monitorData;

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
   * \brief Queries bits per pixel for a format
   *
   * \param [in] Bits per pixel to query
   * \returns Format The DXGI format
   */
  DXGI_FORMAT GetBppMonitorFormat(
          uint32_t                bpp);
  
  /**
   * \brief Converts two display modes
   */
  inline void ConvertDisplayMode(
    const wsi::WsiMode&          WsiMode,
          DXGI_MODE_DESC1*       pDxgiMode) {
    pDxgiMode->Width            = WsiMode.width;
    pDxgiMode->Height           = WsiMode.height;
    pDxgiMode->RefreshRate      = DXGI_RATIONAL{ WsiMode.refreshRate.numerator, WsiMode.refreshRate.denominator };
    pDxgiMode->Format           = GetBppMonitorFormat(WsiMode.bitsPerPixel);
    pDxgiMode->ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    pDxgiMode->Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    pDxgiMode->Stereo           = FALSE;
  }

  /**
   * \brief Converts two display modes
   */
  inline void ConvertDisplayMode(
    const DXGI_MODE_DESC1&        DxgiMode,
          wsi::WsiMode*           pWsiMode) {
    pWsiMode->width        = DxgiMode.Width;
    pWsiMode->height       = DxgiMode.Height;
    pWsiMode->refreshRate  = wsi::WsiRational{ DxgiMode.RefreshRate.Numerator, DxgiMode.RefreshRate.Denominator };
    pWsiMode->bitsPerPixel = GetMonitorFormatBpp(DxgiMode.Format);
    pWsiMode->interlaced   = false;
  }

}