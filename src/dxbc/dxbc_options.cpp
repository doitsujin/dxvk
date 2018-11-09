#include <unordered_map>

#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions() {

  }


  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device) {
    const DxvkDeviceFeatures& devFeatures = device->features();

    useStorageImageReadWithoutFormat = devFeatures.core.features.shaderStorageImageReadWithoutFormat;
    deferKill                        = true;
  }
  
}