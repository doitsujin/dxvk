#pragma once

#include "../dxvk/dxvk_device.h"

namespace dxvk {

  struct D3D11Options;

  enum class DxbcFloatControlFlag : uint32_t {
    DenormFlushToZero32,
    DenormPreserve64,
    PreserveNan32,
    PreserveNan64,
  };

  using DxbcFloatControlFlags = Flags<DxbcFloatControlFlag>;

  struct DxbcOptions {
    DxbcOptions();
    DxbcOptions(const Rc<DxvkDevice>& device, const D3D11Options& options);

    // Clamp oDepth in fragment shaders if the depth
    // clip device feature is not supported
    bool useDepthClipWorkaround = false;

    /// Use the ShaderImageReadWithoutFormat capability.
    bool useStorageImageReadWithoutFormat = false;

    /// Use subgroup operations to reduce the number of
    /// atomic operations for append/consume buffers.
    bool useSubgroupOpsForAtomicCounters = false;

    /// Use a SPIR-V extension to implement D3D-style discards
    bool useDemoteToHelperInvocation = false;

    /// Use subgroup operations to discard fragment
    /// shader invocations if derivatives remain valid.
    bool useSubgroupOpsForEarlyDiscard = false;

    /// Use SDiv instead of SHR to converte byte offsets to
    /// dword offsets. Fixes RE2 and DMC5 on Nvidia drivers.
    bool useSdivForBufferIndex = false;

    /// Enables NaN fixup for render target outputs
    bool enableRtOutputNanFixup = false;

    /// Implement dynamically indexed uniform buffers
    /// with storage buffers for tight bounds checking
    bool dynamicIndexedConstantBufferAsSsbo = false;

    /// Clear thread-group shared memory to zero
    bool zeroInitWorkgroupMemory = false;

    /// Declare vertex positions as invariant
    bool invariantPosition = false;

    /// Insert memory barriers after TGSM stoes
    bool forceTgsmBarriers = false;

    /// Replace ld_ms with ld
    bool disableMsaa = false;

    /// Float control flags
    DxbcFloatControlFlags floatControl;

    /// Minimum storage buffer alignment
    VkDeviceSize minSsboAlignment = 0;
  };
  
}