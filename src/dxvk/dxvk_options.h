#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Enable debug utils
    bool enableDebugUtils;

    /// Enable state cache
    bool enableStateCache;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads;

    /// Enable graphics pipeline library
    Tristate enableGraphicsPipelineLibrary;

    /// Enables pipeline lifetime tracking
    Tristate trackPipelineLifetime;

    /// Shader-related options
    Tristate useRawSsbo;

    /// Maximum memory chunk size in MiB
    int32_t maxChunkSize;

    /// HUD elements
    std::string hud;

    /// Forces swap chain into MAILBOX (if true)
    /// or FIFO_RELAXED (if false) present mode
    Tristate tearFree;

    // Hides integrated GPUs if dedicated GPUs are
    // present. May be necessary for some games that
    // incorrectly assume monitor layouts.
    bool hideIntegratedGraphics;
  };

}
