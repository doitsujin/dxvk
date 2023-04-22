#pragma once

#include "d3d8_include.h"
#include "../d3d9/d3d9_bridge.h"
#include "../util/config/config.h"

namespace dxvk {
  struct D3D8Options {

    /// Remap DEFAULT pool vertex and index buffers to MANAGED to improve
    /// performance by avoiding waiting for games that frequently lock their buffers.
    ///
    /// This implicitly disables direct buffer mapping. Some applications may
    /// need this option disabled to keep from overwriting in-use buffer regions.
    bool managedBufferPlacement = true;

    D3D8Options() {}
    D3D8Options(const Config& config) {
      useShadowBuffers        = config.getOption("d3d8.useShadowBuffers",       useShadowBuffers);
      managedBufferPlacement  = config.getOption("d3d8.managedBufferPlacement", managedBufferPlacement);
    }
  };

}
