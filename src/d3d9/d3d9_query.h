#pragma once

#include "d3d9_device.h"

namespace dxvk {
  class D3D9Query final: public ComObject<IDirect3DQuery9> {
  };
}
