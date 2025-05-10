#include "../util/util_math.h"

#include "d3d9_options.h"

#include "d3d9_caps.h"

namespace dxvk {

  static int32_t parsePciId(const std::string& str) {
    if (str.size() != 4)
      return -1;
    
    int32_t id = 0;

    for (size_t i = 0; i < str.size(); i++) {
      id *= 16;

      if (str[i] >= '0' && str[i] <= '9')
        id += str[i] - '0';
      else if (str[i] >= 'A' && str[i] <= 'F')
        id += str[i] - 'A' + 10;
      else if (str[i] >= 'a' && str[i] <= 'f')
        id += str[i] - 'a' + 10;
      else
        return -1;
    }

    return id;
  }


  D3D9Options::D3D9Options(const Rc<DxvkDevice>& device, const Config& config) {
    const Rc<DxvkAdapter> adapter = device != nullptr ? device->adapter() : nullptr;

    // Fetch these as a string representing a hexadecimal number and parse it.
    this->customVendorId                = parsePciId(config.getOption<std::string>("d3d9.customVendorId"));
    this->customDeviceId                = parsePciId(config.getOption<std::string>("d3d9.customDeviceId"));
    this->customDeviceDesc              = config.getOption<std::string> ("d3d9.customDeviceDesc");

    this->hideNvidiaGpu                 = config.getOption<Tristate>    ("d3d9.hideNvidiaGpu",                 Tristate::Auto) == Tristate::True;
    this->hideNvkGpu                    = config.getOption<Tristate>    ("d3d9.hideNvkGpu",                    Tristate::Auto) == Tristate::True;
    this->hideAmdGpu                    = config.getOption<Tristate>    ("d3d9.hideAmdGpu",                    Tristate::Auto) == Tristate::True;
    this->hideIntelGpu                  = config.getOption<Tristate>    ("d3d9.hideIntelGpu",                  Tristate::True) == Tristate::True;
    this->maxFrameLatency               = config.getOption<int32_t>     ("d3d9.maxFrameLatency",               0);
    this->maxFrameRate                  = config.getOption<int32_t>     ("d3d9.maxFrameRate",                  0);
    this->presentInterval               = config.getOption<int32_t>     ("d3d9.presentInterval",               -1);
    this->shaderModel                   = config.getOption<int32_t>     ("d3d9.shaderModel",                   3u);
    this->dpiAware                      = config.getOption<bool>        ("d3d9.dpiAware",                      true);
    this->strictConstantCopies          = config.getOption<bool>        ("d3d9.strictConstantCopies",          false);
    this->strictPow                     = config.getOption<bool>        ("d3d9.strictPow",                     true);
    this->lenientClear                  = config.getOption<bool>        ("d3d9.lenientClear",                  false);
    this->deferSurfaceCreation          = config.getOption<bool>        ("d3d9.deferSurfaceCreation",          false);
    this->samplerAnisotropy             = config.getOption<int32_t>     ("d3d9.samplerAnisotropy",             -1);
    this->maxAvailableMemory            = config.getOption<int32_t>     ("d3d9.maxAvailableMemory",            4096);
    this->supportDFFormats              = config.getOption<bool>        ("d3d9.supportDFFormats",              true);
    this->supportX4R4G4B4               = config.getOption<bool>        ("d3d9.supportX4R4G4B4",               true);
    this->useD32forD24                  = config.getOption<bool>        ("d3d9.useD32forD24",                  false);
    this->disableA8RT                   = config.getOption<bool>        ("d3d9.disableA8RT",                   false);
    this->invariantPosition             = config.getOption<bool>        ("d3d9.invariantPosition",             true);
    this->memoryTrackTest               = config.getOption<bool>        ("d3d9.memoryTrackTest",               false);
    this->forceSamplerTypeSpecConstants = config.getOption<bool>        ("d3d9.forceSamplerTypeSpecConstants", false);
    this->forceSwapchainMSAA            = config.getOption<int32_t>     ("d3d9.forceSwapchainMSAA",            -1);
    this->forceSampleRateShading        = config.getOption<bool>        ("d3d9.forceSampleRateShading",        false);
    this->forceAspectRatio              = config.getOption<std::string> ("d3d9.forceAspectRatio",              "");
    this->enumerateByDisplays           = config.getOption<bool>        ("d3d9.enumerateByDisplays",           true);
    this->cachedDynamicBuffers          = config.getOption<bool>        ("d3d9.cachedDynamicBuffers",          false);
    this->deviceLocalConstantBuffers    = config.getOption<bool>        ("d3d9.deviceLocalConstantBuffers",    false);
    this->allowDirectBufferMapping      = config.getOption<bool>        ("d3d9.allowDirectBufferMapping",      true);
    this->seamlessCubes                 = config.getOption<bool>        ("d3d9.seamlessCubes",                 false);
    this->textureMemory                 = config.getOption<int32_t>     ("d3d9.textureMemory",                 100) << 20;
    this->deviceLossOnFocusLoss         = config.getOption<bool>        ("d3d9.deviceLossOnFocusLoss",         false);
    this->samplerLodBias                = config.getOption<float>       ("d3d9.samplerLodBias",                0.0f);
    this->clampNegativeLodBias          = config.getOption<bool>        ("d3d9.clampNegativeLodBias",          false);
    this->countLosableResources         = config.getOption<bool>        ("d3d9.countLosableResources",         true);
    this->reproducibleCommandStream     = config.getOption<bool>        ("d3d9.reproducibleCommandStream",     false);
    this->extraFrontbuffer              = config.getOption<bool>        ("d3d9.extraFrontbuffer",              false);

    // D3D8 options
    this->drefScaling                   = config.getOption<int32_t>     ("d3d8.scaleDref",                     0);

    // Clamp the shader model value between 0 and 3
    this->shaderModel    = dxvk::clamp(this->shaderModel, 0u, 3u);
    // Clamp LOD bias so that people don't abuse this in unintended ways
    this->samplerLodBias = dxvk::fclamp(this->samplerLodBias, -2.0f, 1.0f);

    std::string floatEmulation = Config::toLower(config.getOption<std::string>("d3d9.floatEmulation", "auto"));
    if (floatEmulation == "strict") {
      this->d3d9FloatEmulation = D3D9FloatEmulation::Strict;
    } else if (floatEmulation == "false") {
      this->d3d9FloatEmulation = D3D9FloatEmulation::Disabled;
    } else if (floatEmulation == "true") {
      this->d3d9FloatEmulation = D3D9FloatEmulation::Enabled;
    } else {
      bool hasMulz = adapter != nullptr
                  && (adapter->matchesDriver(VK_DRIVER_ID_MESA_RADV)
                   || adapter->matchesDriver(VK_DRIVER_ID_MESA_NVK)
                   || adapter->matchesDriver(VK_DRIVER_ID_AMD_OPEN_SOURCE, Version(2, 0, 316), Version())
                   || adapter->matchesDriver(VK_DRIVER_ID_NVIDIA_PROPRIETARY, Version(565, 57, 1), Version()));
      this->d3d9FloatEmulation = hasMulz ? D3D9FloatEmulation::Strict : D3D9FloatEmulation::Enabled;
    }

    // Intel's hardware sin/cos is so inaccurate that it causes rendering issues in some games
    this->sincosEmulation = adapter && (adapter->matchesDriver(VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA)
                                     || adapter->matchesDriver(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS));
    applyTristate(this->sincosEmulation, config.getOption<Tristate>("d3d9.sincosEmulation", Tristate::Auto));

    this->shaderDumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");
  }

}
