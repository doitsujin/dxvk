#pragma once

#include <windows.h>

#include <array>
#include <vector>
#include <cstdint>

#include "wsi_edid.h"

namespace dxvk::wsi {

  /**
   * \brief Rational number. Eg. 2/3
   */
  struct WsiRational {
    uint32_t numerator;
    uint32_t denominator;
  };

  /**
   * \brief Display mode
   */
  struct WsiMode {
    uint32_t     width;
    uint32_t     height;
    WsiRational  refreshRate;
    uint32_t     bitsPerPixel;
    bool         interlaced;
  };

  /**
    * \brief Default monitor
    *
    * \returns The monitor of given index
    */
  HMONITOR getDefaultMonitor();

  /**
    * \brief Enumerators monitors on the system
    *
    * \returns The monitor of given index
    */
  HMONITOR enumMonitors(uint32_t index);

  /**
    * \brief Enumerators monitors on the system
    * \param [in] adapterLUID array of adapters' LUIDs
    * \param [in] numLUIDs adapterLUID array size (0 for all monitors)
    * \param [in] index Monitor index within enumeration
    *
    * \returns The monitor of given index
    */
  HMONITOR enumMonitors(const LUID *adapterLUID[], uint32_t numLUIDs, uint32_t index);

  /**
    * \brief Get the GDI name of a HMONITOR
    *
    * Get the GDI Device Name of a HMONITOR to
    * return to the end user.
    *
    * This typically looks like \.\\DISPLAY1
    * and has a maximum length of 32 chars.
    *
    * \param [in] hMonitor The monitor
    * \param [out] Name The GDI display name
    * \returns \c true on success, \c false if an error occured
    */
  bool getDisplayName(
          HMONITOR         hMonitor,
          WCHAR            (&Name)[32]);

  /**
    * \brief Get the encompassing coords of a monitor
    */
  bool getDesktopCoordinates(
          HMONITOR         hMonitor,
          RECT*            pRect);

  /**
    * \brief Get the nth display mode
    * 
    * \param [in] hMonitor The monitor
    * \param [in] modeNumber The nth mode
    * \param [out] pMode The resultant mode
    * \returns \c true on success, \c false if the mode could not be found
    */
  bool getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         modeNumber,
          WsiMode*         pMode);

  /**
    * \brief Get the current display mode
    *
    * This is the display mode right now.
    * 
    * \param [in] hMonitor The monitor
    * \param [out] pMode The resultant mode
    * \returns \c true on success, \c false on failure
    */
  bool getCurrentDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode);

  /**
    * \brief Get the current display mode
    *
    * This is the display mode of the user's
    * default desktop.
    * 
    * \param [in] hMonitor The monitor
    * \param [out] pMode The resultant mode
    * \returns \c true on success, \c false on failure
    */
  bool getDesktopDisplayMode(
          HMONITOR         hMonitor,
          WsiMode*         pMode);

  /**
    * \brief Get the size of a monitor
    *
    * Helper function to grab the size of a monitor
    * using getDesktopCoordinates to mirror the window code.
    */
  inline void getMonitorClientSize(
          HMONITOR                hMonitor,
          UINT*                   pWidth,
          UINT*                   pHeight) {
    RECT rect = { };
    getDesktopCoordinates(hMonitor, &rect);

    if (pWidth)
      *pWidth = rect.right - rect.left;
    
    if (pHeight)
      *pHeight = rect.bottom - rect.top;
  }

  /**
    * \brief Get the EDID of a monitor
    *
    * Helper function to grab the EDID of a monitor.
    * This is needed to get the HDR static metadata + colorimetry
    * info of a display for exposing HDR.
    *
    * \param [in] hMonitor The monitor
    * \returns \c EDID if successful, an empty vector if failure.
    */
  WsiEdidData getMonitorEdid(HMONITOR hMonitor);

}
