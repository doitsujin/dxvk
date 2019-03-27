#pragma once

#include "../util/config/config.h"

#include "d3d9_include.h"

namespace dxvk {

  struct D3D9Options {

    D3D9Options(const Config& config);

    /// Present interval. Overrides the value
    /// in D3DPRESENT_PARAMS used in swapchain present.
    int32_t presentInterval;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Set the max shader model the device can support in the caps.
    int32_t shaderModel;

  };

}