#include "ddraw_options.h"

namespace dxvk {

  D3DOptions::D3DOptions(const Config& config) {
    // D3D7/6/5/3/DDraw options
    this->forceMultiThreaded     = config.getOption<bool>   ("ddraw.forceMultiThreaded",     false);
    this->forceSWVP              = config.getOption<bool>   ("ddraw.forceSWVP",              false);
    this->managedVertexBuffers   = config.getOption<bool>   ("ddraw.managedVertexBuffers",   false);
    this->supportR3G3B2          = config.getOption<bool>   ("ddraw.supportR3G3B2",          false);
    this->supportD16             = config.getOption<bool>   ("ddraw.supportD16",              true);
    this->support32BitDepth      = config.getOption<bool>   ("ddraw.support32BitDepth",       true);
    this->useD24X8forD32         = config.getOption<bool>   ("ddraw.useD24X8forD32",         false);
    this->useD16forD24X8         = config.getOption<bool>   ("ddraw.useD16forD24X8",         false);
    this->mask8BitModes          = config.getOption<bool>   ("ddraw.mask8BitModes",          false);
    this->viewportZCorrection    = config.getOption<bool>   ("ddraw.viewportZCorrection",    false);
    this->forceLegacyDiscard     = config.getOption<bool>   ("ddraw.forceLegacyDiscard",     false);
    this->cpuProcessVertices     = config.getOption<bool>   ("ddraw.cpuProcessVertices",      true);
    this->backBufferResize       = config.getOption<bool>   ("ddraw.backBufferResize",        true);
    this->forceLegacyPresent     = config.getOption<bool>   ("ddraw.forceLegacyPresent",     false);
    this->forceRTFlip            = config.getOption<bool>   ("ddraw.forceRTFlip",            false);
    this->forceDCForwarding      = config.getOption<bool>   ("ddraw.forceDCForwarding",      false);
    this->emulateFrontBuffer     = config.getOption<bool>   ("ddraw.emulateFrontBuffer",     false);
    this->ignoreGammaRamp        = config.getOption<bool>   ("ddraw.ignoreGammaRamp",        false);
    this->autoGenMipMaps         = config.getOption<bool>   ("ddraw.autoGenMipMaps",         false);
    this->deviceResourceSharing  = config.getOption<bool>   ("ddraw.deviceResourceSharing",  false);
    this->colorKeyMasking        = config.getOption<bool>   ("ddraw.colorKeyMasking",        false);
    this->legacyDeviceNames      = config.getOption<bool>   ("ddraw.legacyDeviceNames",      false);
    this->nonLocalVideoMemory    = config.getOption<bool>   ("ddraw.nonLocalVideoMemory",     true);
    this->robustTextureLifeCycle = config.getOption<bool>   ("ddraw.robustTextureLifeCycle", false);
    this->apitraceMode           = config.getOption<bool>   ("ddraw.apitraceMode",           false);

    std::string alternatePixelCenterStr = Config::toLower(config.getOption<std::string>("ddraw.alternatePixelCenter", "false"));
    if (alternatePixelCenterStr == "true") {
      this->alternatePixelCenter = AlternatePixelCenter::Enabled;
    } else if (alternatePixelCenterStr == "legacy") {
      this->alternatePixelCenter = AlternatePixelCenter::Legacy;
    } else {
      this->alternatePixelCenter = AlternatePixelCenter::Disabled;
    }

    std::string legacyPresentGuardStr = Config::toLower(config.getOption<std::string>("ddraw.legacyPresentGuard", "auto"));
    if (legacyPresentGuardStr == "strict") {
      this->legacyPresentGuard = D3DLegacyPresentGuard::Strict;
    } else if (legacyPresentGuardStr == "disabled") {
      this->legacyPresentGuard = D3DLegacyPresentGuard::Disabled;
    } else {
      this->legacyPresentGuard = D3DLegacyPresentGuard::Auto;
    }

    std::string emulateFSAAStr = Config::toLower(config.getOption<std::string>("ddraw.emulateFSAA", "false"));
    if (emulateFSAAStr == "true") {
      this->emulateFSAA = FSAAEmulation::Enabled;
    } else if (emulateFSAAStr == "forced") {
      this->emulateFSAA = FSAAEmulation::Forced;
    } else {
      this->emulateFSAA = FSAAEmulation::Disabled;
    }
  };

}
