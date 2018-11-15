#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions(const Config& config);

    /// Allow allocating more memory from
    /// a heap than the device supports.
    bool allowMemoryOvercommit;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads;
  };

}