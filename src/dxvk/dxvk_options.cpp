#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    allowMemoryOvercommit = config.getOption<bool>("dxvk.allowMemoryOvercommit", false);
    asyncPipeCompiler     = config.getOption<bool>("dxvk.asyncPipeCompiler",     false);
  }

}