#pragma once

#include "d3d9_include.h"

namespace dxvk {
  DXGI_FORMAT SurfaceFormatToDXGIFormat(D3DFORMAT Format);
  D3DFORMAT DXGIFormatToSurfaceFormat(DXGI_FORMAT Format);
}
