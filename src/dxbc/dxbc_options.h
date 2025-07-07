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

    /// Determines whether format qualifiers
    /// on typed UAV loads are required
    bool supportsTypedUavLoadR32 = false;

    /// Determines whether raw access chains are supported
    bool supportsRawAccessChains = false;

    /// Whether raw access require a normal access chain
    /// for the binding to work properly
    bool rawAccessChainBug = false;

    /// Declare vertex positions as invariant
    bool invariantPosition = false;

    /// Insert memory barriers after TGSM stoes
    bool forceVolatileTgsmAccess = false;

    /// Try to detect hazards in UAV access and insert
    /// barriers when we know control flow is uniform.
    bool forceComputeUavBarriers = false;

    /// Replace ld_ms with ld
    bool disableMsaa = false;

    /// Force sample rate shading by using sample
    /// interpolation for fragment shader inputs
    bool forceSampleRateShading = false;

    // Enable per-sample interlock if supported
    bool enableSampleShadingInterlock = false;

    /// Whether exporting point size is required
    bool needsPointSizeExport = false;

    /// Whether to enable sincos emulation
    bool sincosEmulation = false;

    /// Whether device suppors 16-bit push constants
    bool supports16BitPushData = false;

    /// Float control flags
    DxbcFloatControlFlags floatControl;

    /// Minimum storage buffer alignment
    VkDeviceSize minSsboAlignment = 0;
  };
  
}
