#pragma once

#include "../util/config/config.h"

namespace dxvk {

  struct DxvkOptions {
    DxvkOptions(const Config& config);

    /// Allow allocating more memory from
    /// a heap than the device supports.
    bool allowMemoryOvercommit;

    /// Enable asynchronous pipeline compilation.
    bool asyncPipeCompiler;
  };

}