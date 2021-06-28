#include "dxgi_monitor.h"

namespace dxvk {

  DxgiMonitorInfo::DxgiMonitorInfo(IUnknown* pParent)
  : m_parent(pParent) {

  }


  DxgiMonitorInfo::~DxgiMonitorInfo() {

  }


  ULONG STDMETHODCALLTYPE DxgiMonitorInfo::AddRef() {
    return m_parent->AddRef();
  }

  
  ULONG STDMETHODCALLTYPE DxgiMonitorInfo::Release() {
    return m_parent->Release();
  }
  

  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_parent->QueryInterface(riid, ppvObject);
  }
  

  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::InitMonitorData(
          HMONITOR                hMonitor,
    const DXGI_VK_MONITOR_DATA*   pData) {
    if (!hMonitor || !pData)
      return E_INVALIDARG;
    
    std::lock_guard<dxvk::mutex> lock(m_monitorMutex);
    auto result = m_monitorData.insert({ hMonitor, *pData });

    return result.second ? S_OK : E_INVALIDARG;
  }


  HRESULT STDMETHODCALLTYPE DxgiMonitorInfo::AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) {
    InitReturnPtr(ppData);

    if (!hMonitor || !ppData)
      return E_INVALIDARG;
    
    m_monitorMutex.lock();

    auto entry = m_monitorData.find(hMonitor);
    if (entry == m_monitorData.end()) {
      m_monitorMutex.unlock();
      return DXGI_ERROR_NOT_FOUND;
    }

    *ppData = &entry->second;
    return S_OK;
  }


  void STDMETHODCALLTYPE DxgiMonitorInfo::ReleaseMonitorData() {
    m_monitorMutex.unlock();
  }


  uint32_t GetMonitorFormatBpp(DXGI_FORMAT Format) {
    switch (Format) {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_B8G8R8X8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
        return 32;
      
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return 64;
      
      default:
        Logger::warn(str::format(
          "GetMonitorFormatBpp: Unknown format: ",
          Format));
        return 32;
    }
  }
  
}