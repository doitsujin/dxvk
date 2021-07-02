#include <unordered_map>

#include "d3d11_options.h"

namespace dxvk {
  
  D3D11Options::D3D11Options(const Config& config, const Rc<DxvkDevice>& device) {
    const DxvkDeviceInfo& devInfo = device->properties();

    this->dcSingleUseMode       = config.getOption<bool>("d3d11.dcSingleUseMode", true);
    this->enableRtOutputNanFixup   = config.getOption<bool>("d3d11.enableRtOutputNanFixup", false);
    this->zeroInitWorkgroupMemory  = config.getOption<bool>("d3d11.zeroInitWorkgroupMemory", false);
    this->forceTgsmBarriers     = config.getOption<bool>("d3d11.forceTgsmBarriers", false);
    this->relaxedBarriers       = config.getOption<bool>("d3d11.relaxedBarriers", false);
    this->maxTessFactor         = config.getOption<int32_t>("d3d11.maxTessFactor", 0);
    this->samplerAnisotropy     = config.getOption<int32_t>("d3d11.samplerAnisotropy", -1);
    this->invariantPosition     = config.getOption<bool>("d3d11.invariantPosition", true);
    this->floatControls         = config.getOption<bool>("d3d11.floatControls", true);
    this->disableMsaa           = config.getOption<bool>("d3d11.disableMsaa", false);
    this->deferSurfaceCreation  = config.getOption<bool>("dxgi.deferSurfaceCreation", false);
    this->numBackBuffers        = config.getOption<int32_t>("dxgi.numBackBuffers", 0);
    this->maxFrameLatency       = config.getOption<int32_t>("dxgi.maxFrameLatency", 0);
    this->maxFrameRate          = config.getOption<int32_t>("dxgi.maxFrameRate", 0);
    this->syncInterval          = config.getOption<int32_t>("dxgi.syncInterval", -1);
    this->tearFree              = config.getOption<Tristate>("dxgi.tearFree", Tristate::Auto);

    this->constantBufferRangeCheck = config.getOption<bool>("d3d11.constantBufferRangeCheck", false)
      && DxvkGpuVendor(devInfo.core.properties.vendorID) != DxvkGpuVendor::Amd;

    bool apitraceAttached = false;
    apitraceAttached = ::GetModuleHandle("dxgitrace.dll") != nullptr;

    this->apitraceMode = config.getOption<bool>("d3d11.apitraceMode", apitraceAttached);

    // Inform user in case they have the option enabled or a game
    // ships a file called dxgitrace.dll for whatever reason.
    if (this->apitraceMode)
      Logger::warn("D3D11: Apitrace mode enabled, may affect performance!");
  }
  
}