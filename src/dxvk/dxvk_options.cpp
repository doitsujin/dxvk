#include "dxvk_options.h"

namespace dxvk {

  DxvkOptions::DxvkOptions(const Config& config) {
    enableDebugUtils      = config.getOption<bool>    ("dxvk.enableDebugUtils",       false);
    enableMemoryDefrag    = config.getOption<Tristate>("dxvk.enableMemoryDefrag",     Tristate::Auto);
    numCompilerThreads    = config.getOption<int32_t> ("dxvk.numCompilerThreads",     0);
    enableGraphicsPipelineLibrary = config.getOption<Tristate>("dxvk.enableGraphicsPipelineLibrary", Tristate::Auto);
    enableDescriptorBuffer = config.getOption<Tristate>("dxvk.enableDescriptorBuffer", Tristate::Auto);
    enableUnifiedImageLayout = config.getOption<bool> ("dxvk.enableUnifiedImageLayouts", true);
    trackPipelineLifetime = config.getOption<Tristate>("dxvk.trackPipelineLifetime",  Tristate::Auto);
    useRawSsbo            = config.getOption<Tristate>("dxvk.useRawSsbo",             Tristate::Auto);
    hud                   = config.getOption<std::string>("dxvk.hud", "");
    tearFree              = config.getOption<Tristate>("dxvk.tearFree",               Tristate::Auto);
    latencySleep          = config.getOption<Tristate>("dxvk.latencySleep",           Tristate::Auto);
    latencyTolerance      = config.getOption<int32_t> ("dxvk.latencyTolerance",       1000);
    disableNvLowLatency2  = config.getOption<Tristate>("dxvk.disableNvLowLatency2",   Tristate::Auto);
    hideIntegratedGraphics = config.getOption<bool>   ("dxvk.hideIntegratedGraphics", false);
    zeroMappedMemory      = config.getOption<bool>    ("dxvk.zeroMappedMemory",       false);
    allowFse              = config.getOption<bool>    ("dxvk.allowFse",               false);
    deviceFilter          = config.getOption<std::string>("dxvk.deviceFilter",        "");
    lowerSinCos           = config.getOption<Tristate>("dxvk.lowerSinCos",            Tristate::Auto);
    tilerMode             = config.getOption<Tristate>("dxvk.tilerMode",              Tristate::Auto);

    auto budget = config.getOption<int32_t>("dxvk.maxMemoryBudget", 0);
    maxMemoryBudget = VkDeviceSize(std::max(budget, 0)) << 20u;
  }

}
