#pragma once

#include "d3d9_include.h"

#include "d3d9_format.h"

#include "../wsi/wsi_window.h"
#include "../wsi/wsi_monitor.h"

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
  bool IsSupportedAdapterFormat(
          D3D9Format Format);

  bool IsSupportedBackBufferFormat(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed);

  bool IsSupportedBackBufferFormat(
          D3D9Format BackBufferFormat);

  inline wsi::WsiMode ConvertDisplayMode(const D3DDISPLAYMODEEX& mode) {
    wsi::WsiMode wsiMode  = { };
    wsiMode.width        = mode.Width;
    wsiMode.height       = mode.Height;
    wsiMode.refreshRate  = wsi::WsiRational{ mode.RefreshRate, 1 };
    wsiMode.bitsPerPixel = GetMonitorFormatBpp(EnumerateFormat(mode.Format));
    wsiMode.interlaced   = mode.ScanLineOrdering == D3DSCANLINEORDERING_INTERLACED;
    return wsiMode;
  }


  inline D3DDISPLAYMODEEX ConvertDisplayMode(const wsi::WsiMode& wsiMode) {
      D3DDISPLAYMODEEX d3d9Mode = { };
      d3d9Mode.Size             = sizeof(D3DDISPLAYMODEEX);
      d3d9Mode.Width            = wsiMode.width;
      d3d9Mode.Height           = wsiMode.height;
      d3d9Mode.RefreshRate      = wsiMode.refreshRate.numerator / wsiMode.refreshRate.denominator;
      d3d9Mode.Format           = D3DFMT_X8R8G8B8;
      d3d9Mode.ScanLineOrdering = wsiMode.interlaced ? D3DSCANLINEORDERING_INTERLACED : D3DSCANLINEORDERING_PROGRESSIVE;
      return d3d9Mode;
  }

}
