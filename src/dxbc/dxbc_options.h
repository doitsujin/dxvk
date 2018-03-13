#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  /**
   * \brief DXBC compiler options
   * 
   * Defines driver- or device-specific options,
   * which are mostly workarounds for driver bugs.
   */
  struct DxbcOptions {
    DxbcOptions() { }
    DxbcOptions(
      const Rc<DxvkDevice>& device);
      
    /// Add extra component to dref coordinate vector
    bool addExtraDrefCoordComponent = false;
      
    /// Use Fmin/Fmax instead of Nmin/Nmax.
    bool useSimpleMinMaxClamp = false;
  };
  
}