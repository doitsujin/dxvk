#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    allowMemoryOvercommit = config.getOption<bool>    ("dxvk.allowMemoryOvercommit",  false);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
  }

}