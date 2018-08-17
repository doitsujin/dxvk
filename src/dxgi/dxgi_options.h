#pragma once

#include "../util/config/config.h"

#include "dxgi_include.h"

namespace dxvk {
  
  /**
   * \brief DXGI options
   * 
   * Per-app options that control the
   * behaviour of some DXGI classes.
   */
  struct DxgiOptions {
    DxgiOptions(const Config& config);

    /// Defer surface creation until first present call. This
    /// fixes issues with games that create multiple swap chains
    /// for a single window that may interfere with each other.
    bool deferSurfaceCreation;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Override PCI vendor and device IDs reported to the
    /// application. This may make apps think they are running
    /// on a different GPU than they do and behave differently.
    int32_t customVendorId;
    int32_t customDeviceId;
  };
  
}
