#include <unordered_map>

#include "d3d11_options.h"

namespace dxvk {
  
  D3D11Options::D3D11Options(const Config& config) {
    this->allowMapFlagNoWait    = config.getOption<bool>("d3d11.allowMapFlagNoWait",    false);
    this->fakeStreamOutSupport  = config.getOption<bool>("d3d11.fakeStreamOutSupport",  false);
    this->maxTessFactor         = config.getOption<int32_t>("d3d11.maxTessFactor",      0);
    this->samplerAnisotropy     = config.getOption<int32_t>("d3d11.samplerAnisotropy",  -1);
  }
  
}