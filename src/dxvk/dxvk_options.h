#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Enable state cache
    bool enableStateCache;

    /// Enables OpenVR loading
    bool enableOpenVR;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads;

    /// Shader-related options
    Tristate useRawSsbo;
    Tristate useEarlyDiscard;

    /// HUD elements
    std::string hud;
  };

}