#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    enableDebugUtils      = config.getOption<bool>    ("dxvk.enableDebugUtils",       false);
    enableStateCache      = config.getOption<bool>    ("dxvk.enableStateCache",       true);
    enableMemoryDefrag    = config.getOption<Tristate>("dxvk.enableMemoryDefrag",     Tristate::Auto);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    enableGraphicsPipelineLibrary = config.getOption<Tristate>("dxvk.enableGraphicsPipelineLibrary", Tristate::Auto);
    trackPipelineLifetime = config.getOption<Tristate>("dxvk.trackPipelineLifetime",  Tristate::Auto);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
    tearFree              = config.getOption<Tristate>("dxvk.tearFree",               Tristate::Auto);
    hideIntegratedGraphics = config.getOption<bool>   ("dxvk.hideIntegratedGraphics", false);
    zeroMappedMemory      = config.getOption<bool>    ("dxvk.zeroMappedMemory",       false);
    allowFse              = config.getOption<bool>    ("dxvk.allowFse",               false);
    deviceFilter          = config.getOption<std::string>("dxvk.deviceFilter",        "");
  }

}
