#include "dxgi_options.h"

#include <unordered_map>

namespace dxvk {

  static int64_t parsePciId(const std::string& str, const size_t size = 4) {
    if (str.size() != size)
      return -1;
    
    int64_t id = 0;

    for (size_t i = 0; i < size; i++) {
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
    // Fetch these as a string representing a hexadecimal number and parse it.
    this->customVendorId = parsePciId(config.getOption<std::string>("dxgi.customVendorId"));
    this->customDeviceId = parsePciId(config.getOption<std::string>("dxgi.customDeviceId"));
    this->customDeviceDesc = config.getOption<std::string>("dxgi.customDeviceDesc", "");

    // Emulate a UMA device
    this->emulateUMA = config.getOption<bool>("dxgi.emulateUMA", false);
    
    // Interpret the memory limits as Megabytes
    this->maxDeviceMemory = VkDeviceSize(config.getOption<int32_t>("dxgi.maxDeviceMemory", 0)) << 20;
    this->maxSharedMemory = VkDeviceSize(config.getOption<int32_t>("dxgi.maxSharedMemory", 0)) << 20;

    this->customSubSysId = parsePciId(config.getOption<std::string>("dxgi.customSubSysId"), 8);
    this->customRevision = parsePciId(config.getOption<std::string>("dxgi.customRevision"), 2);

    this->nvapiHack   = config.getOption<bool>("dxgi.nvapiHack", true);
  }
  
}