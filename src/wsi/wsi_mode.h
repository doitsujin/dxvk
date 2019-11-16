#pragma once

#include <cstdint>

#include <windows.h>

#include "wsi_monitor.h"

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
    WsiRational refreshRate;
    uint32_t     bitsPerPixel;
    bool         interlaced;
  };

  /**
    * \brief Get the nth display mode
    * 
    * \param [in] hMonitor The monitor
    * \param [in] ModeNumber The nth mode
    * \param [out] pMode The resultant mode
    * \returns \c true on success, \c false if the mode could not be found
    */
  bool getDisplayMode(
          HMONITOR         hMonitor,
          uint32_t         ModeNumber,
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

}