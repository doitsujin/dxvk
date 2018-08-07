#pragma once

#include "d3d9_include.h"

namespace dxvk {
  bool SupportedBackBufferFormat(D3DFORMAT Format);

  DXGI_FORMAT BackBufferFormatToDXGIFormat(D3DFORMAT Format);
}
