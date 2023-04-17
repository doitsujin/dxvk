#pragma once

#include "d3d8_include.h"
#include "../d3d9/d3d9_bridge.h"
#include "../util/config/config.h"

namespace dxvk {
  struct D3D8Options {

    /// Remap DEFAULT pool vertex buffers to MANAGED in order to optimize
    /// performance in cases where applications perform read backs
    bool managedBufferPlacement = true;

    D3D8Options() {}
    D3D8Options(const Config& config) {
      useShadowBuffers = config.getOption("d3d8.useShadowBuffers", useShadowBuffers);
      managedBufferPlacement = config.getOption("d3d8.managedBufferPlacement", managedBufferPlacement);
    }
  };

}
