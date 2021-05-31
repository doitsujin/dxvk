#pragma once

#include "./com/com_include.h"

namespace dxvk {

  /**
   * \brief Retrieves primary monitor
   * \returns The primary monitor
   */
  HMONITOR GetDefaultMonitor();

  /**
   * \brief Sets monitor display mode
   *
   * Note that \c pMode may be altered by this function.
   * \param [in] hMonitor The monitor to change
   * \param [in] pMode The desired display mode
   * \returns \c true on success
   */
  BOOL SetMonitorDisplayMode(
          HMONITOR                hMonitor,
          DEVMODEW*               pMode);

  /**
   * \brief Enumerates monitor display modes
   *
   * \param [in] hMonitor The monitor to query
   * \param [in] modeNum Mode number or enum
   * \param [in] pMode The display mode
   * \returns \c true on success
   */
  BOOL GetMonitorDisplayMode(
          HMONITOR                hMonitor,
          DWORD                   modeNum,
          DEVMODEW*               pMode);

  /**
   * \brief Change display modes to registry settings
   * \returns \c true on success
   */
  BOOL RestoreMonitorDisplayMode();

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
  
  /**
   * \brief Queries monitor size
   * 
   * \param [in] hMonitor Monitor to query
   * \param [out] pWidth Client width
   * \param [out] pHeight Client height
   */
  void GetMonitorClientSize(
          HMONITOR                hMonitor,
          UINT*                   pWidth,
          UINT*                   pHeight);

  /**
   * \brief Queries monitor rect
   * 
   * \param [in] hMonitor Monitor to query
   * \param [out] pRect The rect to return
   */
  void GetMonitorRect(
          HMONITOR                hMonitor,
          RECT*                   pRect);

}
