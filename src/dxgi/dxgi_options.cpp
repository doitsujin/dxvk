#include "dxgi_options.h"

#include <unordered_map>

namespace dxvk {

  static int32_t parsePciId(const std::string& str) {
    if (str.size() != 4)
      return -1;
    
    int32_t id = 0;

    for (size_t i = 0; i < str.size(); i++) {
      id *= 16;

      if (str[i] >= '0' && str[i] <= '9')
        id += str[i] - '0';
      else if (str[i] >= 'A' && str[i] <= 'F')
        id += str[i] - 'A' + 10;
      else if (str[i] >= 'a' && str[i] <= 'f')
        id += str[i] - 'a' + 10;
      else
        return -1;
    }

    return id;
  }

  
  DxgiOptions::DxgiOptions(const Config& config) {
    this->deferSurfaceCreation  = config.getOption<bool>    ("dxgi.deferSurfaceCreation", false);
    this->fakeDx10Support       = config.getOption<bool>    ("dxgi.fakeDx10Support",      false);
    this->maxFrameLatency       = config.getOption<int32_t> ("dxgi.maxFrameLatency",      0);
    this->customVendorId        = parsePciId(config.getOption<std::string>("dxgi.customVendorId"));
    this->customDeviceId        = parsePciId(config.getOption<std::string>("dxgi.customDeviceId"));
  }
  
}