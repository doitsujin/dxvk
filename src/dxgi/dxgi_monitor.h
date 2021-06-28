#pragma once

#include <mutex>
#include <unordered_map>

#include "dxgi_interfaces.h"

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

    dxvk::mutex                                        m_monitorMutex;
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

}