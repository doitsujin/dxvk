#include "dxgi_options.h"

#include <unordered_map>

namespace dxvk {
  
  DxgiOptions::DxgiOptions(const Config& config) {
    this->deferSurfaceCreation  = config.getOption<bool>    ("dxgi.deferSurfaceCreation", false);
    this->fakeDx10Support       = config.getOption<bool>    ("dxgi.fakeDx10Support",      false);
    this->maxFrameLatency       = config.getOption<int32_t> ("dxgi.maxFrameLatency",      0);
  }
  
}