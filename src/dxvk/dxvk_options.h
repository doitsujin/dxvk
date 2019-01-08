#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions() { }
    DxvkOptions(const Config& config);

    /// Allow allocating more memory from
    /// a heap than the device supports.
    bool allowMemoryOvercommit;

    /// Enable state cache
    bool enableStateCache;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads;

    /// Shader-related options
    Tristate useRawSsbo;
    Tristate useEarlyDiscard;
  };

}