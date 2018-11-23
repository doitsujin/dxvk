#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {
  
  struct DxbcOptions {
    DxbcOptions();
    DxbcOptions(const Rc<DxvkDevice>& device);

    /// Use the ShaderImageReadWithoutFormat capability.
    bool useStorageImageReadWithoutFormat = false;

    /// Use subgroup operations to discard fragment
    /// shader invocations if derivatives remain valid.
    bool useSubgroupOpsForEarlyDiscard = false;

    /// Use clustered subgroup operations
    bool useSubgroupOpsClustered = false;

    /// Clear thread-group shared memory to zero
    bool zeroInitWorkgroupMemory = false;
  };
  
}