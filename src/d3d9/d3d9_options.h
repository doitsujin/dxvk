#pragma once

#include "../util/config/config.h"

#include "d3d9_include.h"

namespace dxvk {

  struct D3D9Options {

    D3D9Options(const Config& config);

    /// Override PCI vendor and device IDs reported to the
    /// application. This may make apps think they are running
    /// on a different GPU than they do and behave differently.
    int32_t customVendorId;
    int32_t customDeviceId;

    /// Present interval. Overrides the value
    /// in D3DPRESENT_PARAMS used in swapchain present.
    int32_t presentInterval;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Set the max shader model the device can support in the caps.
    int32_t shaderModel;

    /// Whether or not managed resources should stay in memory until unlock, or until manually evicted.
    bool evictManagedOnUnlock;

    /// Whether or not to set the process as DPI aware in Windows when the API interface is created.
    bool dpiAware;

    /// True:  Copy our constant set into UBO if we are relative indexing ever.
    /// False: Copy our constant set into UBO if we are relative indexing at the start of a defined constant
    /// Why?:  In theory, FXC should never generate code where this would be an issue.
    bool strictConstantCopies;

    /// Whether or not we should care about pow(0, 0) = 1
    bool strictPow;

    /// Whether or not to do a fast path clear if we're close enough to the whole render target.
    bool lenientClear;
  };

}