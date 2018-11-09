#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  struct DxbcOptions {
    DxbcOptions();
    DxbcOptions(const Rc<DxvkDevice>& device);

    /// Use the ShaderImageReadWithoutFormat capability.
    /// Enabled by default on GPUs which support this.
    bool useStorageImageReadWithoutFormat = false;
    
    /// Defer kill operation to the end of the shader.
    /// Fixes derivatives that are undefined due to
    /// non-uniform control flow in fragment shaders.
    bool deferKill = false;
  };
  
}