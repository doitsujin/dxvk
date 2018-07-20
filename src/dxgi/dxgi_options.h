#pragma once

#include "dxgi_include.h"

namespace dxvk {
  
  /**
   * \brief DXGI options
   * 
   * Per-app options that control the
   * behaviour of some DXGI classes.
   */
  enum class DxgiOption : uint64_t {
    /// Defer surface creation until first present call. This
    /// fixes issues with games that create multiple swap chains
    /// for a single window that may interfere with each other.
    DeferSurfaceCreation,

    /// Report to the app that Dx10 interfaces are supported,
    /// even if they are not actually supported. Some apps
    /// refuse to start without it, some don't work with it.
    FakeDx10Support,
  };
  
  using DxgiOptions = Flags<DxgiOption>;
  
  /**
   * \brief Gets app-specific DXGI options
   * 
   * \param [in] appName Application name
   * \returns DXGI options for this application
   */
  DxgiOptions getDxgiAppOptions(const std::string& appName);
  
}
