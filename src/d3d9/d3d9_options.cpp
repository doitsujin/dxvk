#include "d3d9_options.h"

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

  D3D9Options::D3D9Options(const Config& config) {
    // Fetch these as a string representing a hexadecimal number and parse it.
    this->customVendorId        = parsePciId(config.getOption<std::string>("d3d9.customVendorId"));
    this->customDeviceId        = parsePciId(config.getOption<std::string>("d3d9.customDeviceId"));

    this->maxFrameLatency       = config.getOption<int32_t>("d3d9.maxFrameLatency", 0);
    this->presentInterval       = config.getOption<int32_t>("d3d9.presentInterval", -1);
    this->shaderModel           = config.getOption<int32_t>("d3d9.shaderModel",     3);
    this->halfPixelOffset       = config.getOption<bool>   ("d3d9.halfPixelOffset", true);
    this->evictManagedOnUnlock  = config.getOption<bool>   ("d3d9.evictManagedOnUnlock", false);
    this->dpiAware              = config.getOption<bool>   ("d3d9.dpiAware", true);
  }

}