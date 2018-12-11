#pragma once

#include <mutex>
#include <unordered_map>

#include "dxgi_include.h"

namespace dxvk {

  class DxgiSwapChain;

  /**
   * \brief Per-monitor data
   */
  struct DXGI_VK_MONITOR_DATA {
    DxgiSwapChain*        pSwapChain;
    DXGI_FRAME_STATISTICS FrameStats;
    DXGI_GAMMA_CONTROL    GammaCurve;
  };


  /**
   * \brief Initializes monitor data
   * 
   * Fails if data for the given monitor already exists.
   * \param [in] hMonitor The monitor handle
   * \param [in] pData Initial data
   */
  HRESULT InitMonitorData(
          HMONITOR                hMonitor,
    const DXGI_VK_MONITOR_DATA*   pData);

  /**
   * \brief Retrieves and locks monitor data
   * 
   * Fails if no data for the given monitor exists.
   * \param [in] hMonitor The monitor handle
   * \param [out] Pointer to monitor data
   * \returns S_OK on success
   */
  HRESULT AcquireMonitorData(
          HMONITOR                hMonitor,
          DXGI_VK_MONITOR_DATA**  ppData);
  
  /**
   * \brief Unlocks monitor data
   * 
   * Must be called after each successful
   * call to \ref AcquireMonitorData.
   * \param [in] hMonitor The monitor handle
   */
  void ReleaseMonitorData();

}