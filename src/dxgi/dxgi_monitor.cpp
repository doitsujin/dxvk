#include "dxgi_monitor.h"

namespace dxvk {

  std::mutex                                         g_monitorMutex;
  std::unordered_map<HMONITOR, DXGI_VK_MONITOR_DATA> g_monitorData;


  uint32_t GetMonitorFormatBpp(DXGI_FORMAT Format) {
    switch (Format) {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
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


  HRESULT InitMonitorData(
          HMONITOR                hMonitor,
    const DXGI_VK_MONITOR_DATA*   pData) {
    if (!hMonitor || !pData)
      return E_INVALIDARG;
    
    std::lock_guard<std::mutex> lock(g_monitorMutex);
    auto result = g_monitorData.insert({ hMonitor, *pData });

    return result.second ? S_OK : E_INVALIDARG;
  }


  HRESULT AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData) {
    InitReturnPtr(ppData);

    if (!hMonitor || !ppData)
      return E_INVALIDARG;
    
    g_monitorMutex.lock();

    auto entry = g_monitorData.find(hMonitor);
    if (entry == g_monitorData.end()) {
      g_monitorMutex.unlock();
      return DXGI_ERROR_NOT_FOUND;
    }

    *ppData = &entry->second;
    return S_OK;
  }

  
  void ReleaseMonitorData() {
    g_monitorMutex.unlock();
  }
  

}