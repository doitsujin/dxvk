#pragma once

#include "d3d9_include.h"

namespace dxvk {
  // Fills a D3D9 capabilities structure.
  void FillCaps(UINT adapter, D3DCAPS9& caps);
}
