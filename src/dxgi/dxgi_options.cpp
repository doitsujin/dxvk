#include "dxgi_options.h"

#include <unordered_map>

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

  /* First generation XeSS causes crash on proton for Intel due to missing
   * Intel interface. Avoid crash by pretending to be non-Intel if the
   * libxess.dll module is loaded by an application.
   */
  static bool isXessUsed() {
#ifdef _WIN32
      if (GetModuleHandleA("libxess") != nullptr ||
          GetModuleHandleA("libxess_dx11") != nullptr)
        return true;
      else
        return false;
#else
      return false;
#endif
  }

  static bool isNvapiEnabled() {
    return env::getEnvVar("DXVK_ENABLE_NVAPI") == "1";
  }


  static bool isHDRDisallowed(bool enableUe4Workarounds) {
#ifdef _WIN32
    // Unreal Engine 4 titles use AGS/NVAPI to try and enable
    // HDR globally.
    // The game checks IDXGIOutput::GetDesc1's ColorSpace
    // being HDR10 to see if it should enable HDR.
    // Many of these UE4 games statically link against AGS.
    //
    // This is a problem as when UE4 tries to enable HDR via AGS,
    // it does not check if AGSContext, and the display info etc
    // are nullptr unlike the rest of the code using AGS.
    // So we need to special-case UE4 titles to disable reporting a HDR
    // when they are in DX11 mode.
    //
    // The simplest way to do this is to key off the fact that all
    // UE4 titles have an executable ending with "-Win64-Shipping".
    //
    // We check if d3d12.dll is present, to determine what path in
    // UE4 we are on, as there are some games that ship both and support HDR.
    // (eg. The Dark Pictures: House of Ashes, 1281590)
    // Luckily for us, they only load d3d12.dll on the D3D12 render path
    // so we can key off that to force disable HDR only in D3D11.
    std::string exeName = env::getExeName();
    bool isUE4 = enableUe4Workarounds || exeName.find("-Win64-Shipping") != std::string::npos;
    bool hasD3D12 = GetModuleHandleA("d3d12") != nullptr;

    if (isUE4 && !hasD3D12 && !isNvapiEnabled())
      return true;
#endif
    return false;
  }

  
  DxgiOptions::DxgiOptions(const Config& config) {
    // Fetch these as a string representing a hexadecimal number and parse it.
    this->customVendorId = parsePciId(config.getOption<std::string>("dxgi.customVendorId"));
    this->customDeviceId = parsePciId(config.getOption<std::string>("dxgi.customDeviceId"));
    this->customDeviceDesc = config.getOption<std::string>("dxgi.customDeviceDesc", "");
    
    // Interpret the memory limits as Megabytes
    this->maxDeviceMemory = VkDeviceSize(config.getOption<int32_t>("dxgi.maxDeviceMemory", 0)) << 20;
    this->maxSharedMemory = VkDeviceSize(config.getOption<int32_t>("dxgi.maxSharedMemory", 0)) << 20;

    this->maxFrameRate = config.getOption<int32_t>("dxgi.maxFrameRate", 0);
    this->syncInterval = config.getOption<int32_t>("dxgi.syncInterval", -1);

    // We don't support dcomp swapchains and some games may rely on them failing on creation
    this->enableDummyCompositionSwapchain = config.getOption<bool>("dxgi.enableDummyCompositionSwapchain", false);

    // Expose Nvidia GPUs properly if NvAPI is enabled in environment
    this->hideNvidiaGpu = !isNvapiEnabled();
    applyTristate(this->hideNvidiaGpu, config.getOption<Tristate>("dxgi.hideNvidiaGpu", Tristate::Auto));

    // Treat NVK adapters the same as Nvidia cards on the proprietary by
    // default, but provide an override in case something isn't working.
    this->hideNvkGpu = this->hideNvidiaGpu;
    applyTristate(this->hideNvkGpu, config.getOption<Tristate>("dxgi.hideNvkGpu", Tristate::Auto));

    // Expose AMD and Intel GPU by default, unless a config override is active.
    // Implement as a tristate so that we have the option to introduce similar
    // logic to Nvidia later, if necessary.
    this->hideAmdGpu = config.getOption<Tristate>("dxgi.hideAmdGpu", Tristate::Auto) == Tristate::True;
    this->hideIntelGpu = config.getOption<Tristate>("dxgi.hideIntelGpu", Tristate::Auto) == Tristate::True;

    /* Force vendor ID to non-Intel ID when XeSS is in use */
    if (isXessUsed()) {
      Logger::info(str::format("Detected XeSS usage, hiding Intel GPU Vendor"));
      this->hideIntelGpu = true;
    }

    this->enableHDR = config.getOption<bool>("dxgi.enableHDR", env::getEnvVar("DXVK_HDR") == "1");

    bool enableUe4Workarounds = config.getOption<bool>("dxgi.enableUe4Workarounds", false);

    if (this->enableHDR && isHDRDisallowed(enableUe4Workarounds)) {
      Logger::info("HDR was configured to be enabled, but has been force disabled as a UE4 DX11 game was detected.");
      this->enableHDR = false;
    }
  }
  
}
