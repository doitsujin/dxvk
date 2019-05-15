#pragma once

#include "d3d9_include.h"

#include "d3d9_format.h"

namespace dxvk {

  /**
  * \brief Queries bits per pixel for a format
  *
  * The format must be a valid swap chain format.
  * \param [in] Format The D3D9 format to query
  * \returns Bits per pixel for this format
  */
  uint32_t GetMonitorFormatBpp(
    D3D9Format             Format);

  /**
  * \brief Returns if a format is supported for a backbuffer/swapchain.
  *
  * \param [in] Format The D3D9 format to query
  * \returns If it is supported as a swapchain/backbuffer format.
  */
  bool IsSupportedMonitorFormat(
          D3D9Format Format,
          BOOL       Windowed);

  bool IsSupportedBackBufferFormat(
          D3D9Format BackBufferFormat,
          BOOL       Windowed);

  bool IsSupportedBackBufferFormat(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed);

  HMONITOR GetDefaultMonitor();

  /**
   * \brief Sets monitor display mode
   * 
   * \param [in] hMonitor Monitor handle
   * \param [in] pMode Display mode properties
   * \returns S_OK on success
   */
  HRESULT SetMonitorDisplayMode(
          HMONITOR                hMonitor,
    const D3DDISPLAYMODEEX*       pMode);
  
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