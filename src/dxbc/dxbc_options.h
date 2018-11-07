#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  struct DxbcOptions {
    DxbcOptions();
    DxbcOptions(const Rc<DxvkDevice>& device);

    /// Use the ShaderImageReadWithoutFormat capability.
    bool useStorageImageReadWithoutFormat = false;
  };
  
}