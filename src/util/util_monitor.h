#pragma once

#include "./com/com_include.h"

namespace dxvk {

  /**
   * \brief Retrieves primary monitor
   * \returns The primary monitor
   */
  HMONITOR GetDefaultMonitor();

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
