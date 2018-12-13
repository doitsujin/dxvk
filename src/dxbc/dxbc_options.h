#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {

  struct D3D11Options;
  
  struct DxbcOptions {
    DxbcOptions();
    DxbcOptions(const Rc<DxvkDevice>& device, const D3D11Options& options);

    /// Use the ShaderImageReadWithoutFormat capability.
    bool useStorageImageReadWithoutFormat = false;

    /// Use subgroup operations to discard fragment
    /// shader invocations if derivatives remain valid.
    bool useSubgroupOpsForEarlyDiscard = false;

    /// Use SSBOs instead of texel buffers
    /// for raw and structured buffers.
    bool useRawSsbo = false;

    /// Clear thread-group shared memory to zero
    bool zeroInitWorkgroupMemory = false;
  };
  
}