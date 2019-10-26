#include <unordered_map>

#include "d3d11_options.h"

namespace dxvk {
  
  D3D11Options::D3D11Options(const Config& config) {
    this->allowMapFlagNoWait    = config.getOption<bool>("d3d11.allowMapFlagNoWait", true);
    this->dcSingleUseMode       = config.getOption<bool>("d3d11.dcSingleUseMode", true);
    this->strictDivision           = config.getOption<bool>("d3d11.strictDivision", false);
    this->constantBufferRangeCheck = config.getOption<bool>("d3d11.constantBufferRangeCheck", false);
    this->zeroInitWorkgroupMemory  = config.getOption<bool>("d3d11.zeroInitWorkgroupMemory", false);
    this->relaxedBarriers       = config.getOption<bool>("d3d11.relaxedBarriers", false);
    this->maxTessFactor         = config.getOption<int32_t>("d3d11.maxTessFactor", 0);
    this->samplerAnisotropy     = config.getOption<int32_t>("d3d11.samplerAnisotropy", -1);
    this->deferSurfaceCreation  = config.getOption<bool>("dxgi.deferSurfaceCreation", false);
    this->numBackBuffers        = config.getOption<int32_t>("dxgi.numBackBuffers", 0);
    this->maxFrameLatency       = config.getOption<int32_t>("dxgi.maxFrameLatency", 0);
    this->syncInterval          = config.getOption<int32_t>("dxgi.syncInterval", -1);

    bool apitraceAttached = false;
    #ifndef __WINE__
    apitraceAttached = ::GetModuleHandle("dxgitrace.dll") != nullptr;
    #endif

    this->apitraceMode = config.getOption<bool>("d3d11.apitraceMode", apitraceAttached);

    // Inform user in case they have the option enabled or a game
    // ships a file called dxgitrace.dll for whatever reason.
    if (this->apitraceMode)
      Logger::warn("D3D11: Apitrace mode enabled, may affect performance!");
  }
  
}