#pragma once

#include "../dxvk/dxvk_device.h"
#include "../d3d9/d3d9_options.h"

namespace dxvk {

  struct D3D9Options;

  struct DxsoOptions {
    DxsoOptions();
    DxsoOptions(const Rc<DxvkDevice>& device, const D3D9Options& options);

    /// Use a SPIR-V extension to implement D3D-style discards
    bool useDemoteToHelperInvocation = false;

    /// Use subgroup operations to discard fragment
    /// shader invocations if derivatives remain valid.
    bool useSubgroupOpsForEarlyDiscard = false;

    /// True:  Copy our constant set into UBO if we are relative indexing ever.
    /// False: Copy our constant set into UBO if we are relative indexing at the start of a defined constant
    /// Why?:  In theory, FXC should never generate code where this would be an issue.
    bool strictConstantCopies;

    /// Whether to emulate d3d9 float behaviour using clampps
    /// True:  Perform emulation to emulate behaviour (ie. anything * 0 = 0)
    /// False: Don't do anything.
    bool d3d9FloatEmulation;

    /// Whether or not we should care about pow(0, 0) = 1
    bool strictPow;

    /// Max version of shader to support
    uint32_t shaderModel;
  };

}