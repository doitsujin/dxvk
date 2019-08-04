#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    enableStateCache      = config.getOption<bool>    ("dxvk.enableStateCache",       true);
    enableTransferQueue   = config.getOption<bool>    ("dxvk.enableTransferQueue",    true);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    asyncPresent          = config.getOption<Tristate>("dxvk.asyncPresent",           Tristate::Auto);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    useEarlyDiscard       = config.getOption<Tristate>("dxvk.useEarlyDiscard",        Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
  }

}