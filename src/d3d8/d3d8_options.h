#pragma once

#include "d3d8_include.h"
#include "../d3d9/d3d9_bridge.h"
#include "../util/config/config.h"

namespace dxvk {
  struct D3D8Options {

    // HACK: Force 16-bit depth buffer (D16) to reduce Z-fighting in some cases 
    bool forceD16 = false;

    D3D8Options() {}
    D3D8Options(const Config& config) {
      forceD16 = config.getOption("d3d8.forceD16", forceD16);
    }
  };

}
