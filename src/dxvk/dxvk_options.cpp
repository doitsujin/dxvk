#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    enableDebugUtils      = config.getOption<bool>    ("dxvk.enableDebugUtils",       false);
    enableStateCache      = config.getOption<bool>    ("dxvk.enableStateCache",       true);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    shrinkNvidiaHvvHeap   = config.getOption<Tristate>("dxvk.shrinkNvidiaHvvHeap",    Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
    enableAsync           = config.getOption<bool>    ("dxvk.enableAsync",            false);
    numAsyncThreads       = config.getOption<int32_t> ("dxvk.numAsyncThreads",        0);
  }

}
