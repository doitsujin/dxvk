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
   * \brief Queries bits per pixel for a format
   * 
   * The format must be a valid swap chain format.
   * \param [in] Format The DXGI format to query
   * \returns Bits per pixel for this format
   */
  uint32_t GetMonitorFormatBpp(
          DXGI_FORMAT             Format);
  
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


  /**
   * \brief Retrieves monitor display mode
   *
   * \param [in] hMonitor Monitor handle
   * \param [in] ModeNum Mode number
   * \param [out] Display mode properties
   * \returns S_OK on success
   */
  HRESULT GetMonitorDisplayMode(
          HMONITOR                hMonitor,
          DWORD                   ModeNum,
          DXGI_MODE_DESC*         pMode);

  /**
   * \brief Sets monitor display mode
   * 
   * \param [in] hMonitor Monitor handle
   * \param [in] pMode Display mode properties
   * \returns S_OK on success
   */
  HRESULT SetMonitorDisplayMode(
          HMONITOR                hMonitor,
    const DXGI_MODE_DESC*         pMode);
  
  /**
   * \brief Queries window client size
   * 
   * \param [in] hWnd Window to query
   * \param [out] pWidth Client width
   * \param [out] pHeight Client height
   */
  void GetWindowClientSize(
          HWND                    hWnd,
          UINT*                   pWidth,
          UINT*                   pHeight);

}