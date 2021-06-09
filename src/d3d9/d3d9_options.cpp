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
    this->customVendorId        = parsePciId(config.getOption<std::string>("d3d9.customVendorId"));
    this->customDeviceId        = parsePciId(config.getOption<std::string>("d3d9.customDeviceId"));
    this->customDeviceDesc      =            config.getOption<std::string>("d3d9.customDeviceDesc");

    const int32_t vendorId = this->customDeviceId != -1
      ? this->customDeviceId
      : (adapter != nullptr ? adapter->deviceProperties().vendorID : 0);

    this->maxFrameLatency               = config.getOption<int32_t>     ("d3d9.maxFrameLatency",               0);
    this->maxFrameRate                  = config.getOption<int32_t>     ("d3d9.maxFrameRate",                  0);
    this->presentInterval               = config.getOption<int32_t>     ("d3d9.presentInterval",               -1);
    this->shaderModel                   = config.getOption<int32_t>     ("d3d9.shaderModel",                   3);
    this->evictManagedOnUnlock          = config.getOption<bool>        ("d3d9.evictManagedOnUnlock",          false);
    this->dpiAware                      = config.getOption<bool>        ("d3d9.dpiAware",                      true);
    this->strictConstantCopies          = config.getOption<bool>        ("d3d9.strictConstantCopies",          false);
    this->strictPow                     = config.getOption<bool>        ("d3d9.strictPow",                     true);
    this->lenientClear                  = config.getOption<bool>        ("d3d9.lenientClear",                  false);
    this->numBackBuffers                = config.getOption<int32_t>     ("d3d9.numBackBuffers",                0);
    this->noExplicitFrontBuffer         = config.getOption<bool>        ("d3d9.noExplicitFrontBuffer",         false);
    this->deferSurfaceCreation          = config.getOption<bool>        ("d3d9.deferSurfaceCreation",          false);
    this->samplerAnisotropy             = config.getOption<int32_t>     ("d3d9.samplerAnisotropy",             -1);
    this->maxAvailableMemory            = config.getOption<int32_t>     ("d3d9.maxAvailableMemory",            4096);
    this->supportDFFormats              = config.getOption<bool>        ("d3d9.supportDFFormats",              true);
    this->supportX4R4G4B4               = config.getOption<bool>        ("d3d9.supportX4R4G4B4",               true);
    this->supportD32                    = config.getOption<bool>        ("d3d9.supportD32",                    true);
    this->swvpFloatCount                = config.getOption<int32_t>     ("d3d9.swvpFloatCount",                caps::MaxFloatConstantsSoftware);
    this->swvpIntCount                  = config.getOption<int32_t>     ("d3d9.swvpIntCount",                  caps::MaxOtherConstantsSoftware);
    this->swvpBoolCount                 = config.getOption<int32_t>     ("d3d9.swvpBoolCount",                 caps::MaxOtherConstantsSoftware);
    this->disableA8RT                   = config.getOption<bool>        ("d3d9.disableA8RT",                   false);
    this->invariantPosition             = config.getOption<bool>        ("d3d9.invariantPosition",             false);
    this->memoryTrackTest               = config.getOption<bool>        ("d3d9.memoryTrackTest",               false);
    this->supportVCache                 = config.getOption<bool>        ("d3d9.supportVCache",                 vendorId == 0x10de);
    this->enableDialogMode              = config.getOption<bool>        ("d3d9.enableDialogMode",              false);
    this->forceSamplerTypeSpecConstants = config.getOption<bool>        ("d3d9.forceSamplerTypeSpecConstants", false);
    this->forceSwapchainMSAA            = config.getOption<int32_t>     ("d3d9.forceSwapchainMSAA",            -1);
    this->forceAspectRatio              = config.getOption<std::string> ("d3d9.forceAspectRatio",              "");
    this->allowDoNotWait                = config.getOption<bool>        ("d3d9.allowDoNotWait",                true);
    this->allowDiscard                  = config.getOption<bool>        ("d3d9.allowDiscard",                  true);
    this->enumerateByDisplays           = config.getOption<bool>        ("d3d9.enumerateByDisplays",           true);
    this->longMad                       = config.getOption<bool>        ("d3d9.longMad",                       false);
    this->tearFree                      = config.getOption<Tristate>    ("d3d9.tearFree",                      Tristate::Auto);
    this->alphaTestWiggleRoom           = config.getOption<bool>        ("d3d9.alphaTestWiggleRoom",           false);
    this->apitraceMode                  = config.getOption<bool>        ("d3d9.apitraceMode",                  false);
    this->deviceLocalConstantBuffers    = config.getOption<bool>        ("d3d9.deviceLocalConstantBuffers",    false);

    // If we are not Nvidia, enable general hazards.
    this->generalHazards = adapter != nullptr
                        && !adapter->matchesDriver(
                            DxvkGpuVendor::Nvidia,
                            VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR,
                            0, 0);
    applyTristate(this->generalHazards, config.getOption<Tristate>("d3d9.generalHazards", Tristate::Auto));

    this->d3d9FloatEmulation = true; // <-- Future Extension?
    applyTristate(this->d3d9FloatEmulation, config.getOption<Tristate>("d3d9.floatEmulation", Tristate::Auto));
  }

}