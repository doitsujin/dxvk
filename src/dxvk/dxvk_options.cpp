#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    enableStateCache      = config.getOption<bool>    ("dxvk.enableStateCache",       true);
    enableOpenVR          = config.getOption<bool>    ("dxvk.enableOpenVR",           true);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    useEarlyDiscard       = config.getOption<Tristate>("dxvk.useEarlyDiscard",        Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
  }

}