#pragma once

#include "../util/config/config.h"

#include "../vulkan/vulkan_loader.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Enable debug utils
    bool enableDebugUtils = false;

    /// Enable memory defragmentation
    Tristate enableMemoryDefrag = Tristate::Auto;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads = 0;

    /// Enable graphics pipeline library
    Tristate enableGraphicsPipelineLibrary = Tristate::Auto;

    /// Enable descriptor heap
    Tristate enableDescriptorHeap = Tristate::Auto;

    /// Enable descriptor buffer
    Tristate enableDescriptorBuffer = Tristate::Auto;

    /// Enable unified image layout path
    bool enableUnifiedImageLayout = true;

    /// Enables pipeline lifetime tracking
    Tristate trackPipelineLifetime = Tristate::Auto;

    /// Shader-related options
    Tristate useRawSsbo = Tristate::Auto;

    /// HUD elements
    std::string hud;

    /// Forces swap chain into MAILBOX (if true)
    /// or FIFO_RELAXED (if false) present mode
    Tristate tearFree = Tristate::Auto;

    /// Enables latency sleep
    Tristate latencySleep = Tristate::Auto;

    /// Latency tolerance, in microseconds
    int32_t latencyTolerance = 0u;

    /// Disable VK_NV_low_latency2. This extension
    /// appears to be all sorts of broken on 32-bit.
    Tristate disableNvLowLatency2 = Tristate::Auto;

    // Hides integrated GPUs if dedicated GPUs are
    // present. May be necessary for some games that
    // incorrectly assume monitor layouts.
    bool hideIntegratedGraphics = false;

    /// Clears all mapped memory to zero.
    bool zeroMappedMemory = false;

    /// Allows full-screen exclusive mode on Windows
    bool allowFse = false;

    /// Whether to enable tiler optimizations
    Tristate tilerMode = Tristate::Auto;

    /// Overrides memory budget for DXVK
    VkDeviceSize maxMemoryBudget = 0u;

    /// Whether to use custom sin/cos approximation
    Tristate lowerSinCos = Tristate::Auto;

    /// Enables implicit resolves that are used to
    /// deal with MSAA-related undefined behaviour.
    bool enableImplicitResolves = true;

    /// Device name
    std::string deviceFilter;

    /// Fragment shading rate applied to additive/multiplicative blend
    /// pipelines (fire/glow/smoke/particles). Defaults to {2,2} (4x FS
    /// reduction). Values: Off / 1x1 (patch disabled), 2x1 / 1x2 (2x
    /// reduction, less visible pixelation), 2x2 (4x reduction), 4x2 /
    /// 2x4 / 4x4 (8x-16x reduction, hardware-dependent and visibly
    /// blocky on small effects). Parsed from "dxvk.transparentShadingRate".
    /// Validated against device khrFragmentShadingRate.maxFragmentSize;
    /// invalid or unsupported values fall back to {1,1}.
    VkExtent2D transparentShadingRate = { 2u, 2u };
  };

}
