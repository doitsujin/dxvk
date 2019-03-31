#include "d3d9_options.h"

namespace dxvk {

  D3D9Options::D3D9Options(const Config& config) {
    this->maxFrameLatency       = config.getOption<int32_t>("d3d9.maxFrameLatency", 0);
    this->presentInterval       = config.getOption<int32_t>("d3d9.presentInterval", -1);
    this->shaderModel           = config.getOption<int32_t>("d3d9.shaderModel",     2);
    this->halfPixelOffset       = config.getOption<bool>   ("d3d9.halfPixelOffset", true);
  }

}