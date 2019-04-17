#pragma once

#include "../dxvk/dxvk_device.h"
#include "../d3d9/d3d9_options.h"

namespace dxvk {

  struct D3D9Options;

  struct DxsoOptions {
    DxsoOptions();
    DxsoOptions(const Rc<DxvkDevice>& device, const D3D9Options& options);

    /// Use subgroup operations to discard fragment
    /// shader invocations if derivatives remain valid.
    bool useSubgroupOpsForEarlyDiscard = false;
  };

}