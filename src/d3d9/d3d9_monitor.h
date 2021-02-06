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
  bool IsSupportedAdapterFormat(
          D3D9Format Format);

  bool IsSupportedBackBufferFormat(
          D3D9Format AdapterFormat,
          D3D9Format BackBufferFormat,
          BOOL       Windowed);

  bool IsSupportedBackBufferFormat(
          D3D9Format BackBufferFormat);
}
