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

    /// Enables OpenXR loading
    bool enableOpenXR;

    /// Number of compiler threads
    /// when using the state cache
    int32_t numCompilerThreads;

    /// Shader-related options
    Tristate useRawSsbo;

    /// Workaround for NVIDIA driver
    /// bug 3114283. Cut usable HVV
    /// (Host-Visible Vidmem) heap
    /// in half to avoid crash
    Tristate halveNvidiaHVVHeap;

    /// HUD elements
    std::string hud;
  };

}
