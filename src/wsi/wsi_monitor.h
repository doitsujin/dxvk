#pragma once

#include <windows.h>

#include <array>

namespace dxvk::wsi {

  /**
    * \brief Enumerators monitors on the system
    *
    * \returns The monitor of given index
    */
  HMONITOR enumMonitors(uint32_t index);

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
  
}