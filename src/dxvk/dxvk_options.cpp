#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    enableStateCache      = config.getOption<bool>    ("dxvk.enableStateCache",       true);
    enableOpenVR          = config.getOption<bool>    ("dxvk.enableOpenVR",           true);
    enableOpenXR          = config.getOption<bool>    ("dxvk.enableOpenXR",           true);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    halveNvidiaHVVHeap    = config.getOption<Tristate>("dxvk.halveNvidiaHVVHeap",     Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
  }

}
