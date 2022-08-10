#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Enable debug utils (alternative to DXVK_PERF_EVENTS=1)
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

    /// HUD elements
    std::string hud;
  };

}
