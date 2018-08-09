#pragma once

#include "d3d9_include.h"

namespace dxvk {
  /// Checks if a given format is considered legal as a back buffer format in D3D9.
  bool IsBackBufferFormat(D3DFORMAT Format);
  /// Checks if a given format is considered legal for depth / stencil buffers in D3D9.
  bool IsDepthStencilFormat(D3DFORMAT Format);

  /// Converts a D3D9 format to a DXGI format.
  DXGI_FORMAT SurfaceFormatToDXGIFormat(D3DFORMAT Format);
  /// Converts a DXGI format to a D3D9 format.
  D3DFORMAT DXGIFormatToSurfaceFormat(DXGI_FORMAT Format);
}
