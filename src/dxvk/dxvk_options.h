#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Enable debug utils
    bool enableDebugUtils = false;

    /// Enable state cache
    bool enableStateCache = true;

    /// Enable memory defragmentation
    Tristate enableMemoryDefrag = Tristate::Auto;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads = 0;

    /// Enable graphics pipeline library
    Tristate enableGraphicsPipelineLibrary = Tristate::Auto;

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

    // Device name
    std::string deviceFilter;
  };

}
