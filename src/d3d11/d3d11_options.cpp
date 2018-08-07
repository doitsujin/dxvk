#include <unordered_map>

#include "d3d11_options.h"

namespace dxvk {
  
  D3D11Options::D3D11Options(const Config& config) {
    this->allowMapFlagNoWait    = config.getOption<bool>("d3d11.allowMapFlagNoWait",    false);
    this->fakeStreamOutSupport  = config.getOption<bool>("d3d11.fakeStreamOutSupport",  false);
  }
  
}