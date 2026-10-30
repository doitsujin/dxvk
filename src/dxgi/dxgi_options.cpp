#include "dxgi_options.h"

#include <cstring>
#include <unordered_map>

namespace dxvk {

  #ifdef _WIN32
  static bool isVersionAtLeast(uint32_t major, uint32_t minor, uint32_t patch, uint32_t build,
          uint32_t reqMajor, uint32_t reqMinor, uint32_t reqPatch, uint32_t reqBuild) {
    if (major != reqMajor)
      return major > reqMajor;
    if (minor != reqMinor)
      return minor > reqMinor;
    if (patch != reqPatch)
      return patch > reqPatch;
    return build >= reqBuild;
  }
  #endif

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

  /* Some XeSS versions crash on Proton for Intel due to missing Intel
   * interfaces. Avoid the crash by pretending to be non-Intel once such
   * a libxess.dll module is loaded by the application. Applications may
   * load XeSS late, e.g. the Unreal Engine plugin loads it only after the
   * D3D12 device is created, so this can't be decided up front. */
  static XessVendorWa getXessVendorWa() {
    #ifdef _WIN32
    HMODULE libxess = nullptr;

    // Use Ex variant here to keep the library loaded while we use the handle
    if (!GetModuleHandleExA(0u, "libxess", &libxess)
     && !GetModuleHandleExA(0u, "libxess_dx11", &libxess))
      return XessVendorWa::Unknown;

    if (!libxess)
      return XessVendorWa::Unknown;

    // Wine builtin XeSS (e.g. Proton forwarding to a native libxess.so)
    // doesn't use the Intel path that needs the workaround.
    static const char builtinSignature[] = "Wine builtin DLL";
    bool isBuiltin = !std::memcmp(reinterpret_cast<const char*>(libxess) + 0x40,
      builtinSignature, sizeof(builtinSignature));

    // For some reason it seems to be impossible to just query
    // the length, need to pre-allocate for the worst case
    std::array<char, MAX_PATH + 1u> fileName = {};
    auto nameLen = GetModuleFileNameA(libxess, fileName.data(), fileName.size());

    // Should be safe to release the module now, we only operate
    // on the actual file from now on
    FreeLibrary(libxess);

    if (isBuiltin)
      return XessVendorWa::NotNeeded;

    if (!nameLen) {
      Logger::warn("DXGI: Failed to get file name for XeSS module");
      return XessVendorWa::Needed;
    }

    // Query version info blob size...
    auto fiSize = GetFileVersionInfoSizeA(fileName.data(), nullptr);

    if (!fiSize) {
      Logger::warn("DXGI: Failed to get XeSS version info size");
      return XessVendorWa::Needed;
    }

    // Retrieve actual version info blob
    std::vector<char> fiData(fiSize);

    if (!GetFileVersionInfoA(fileName.data(), 0u, fiSize, fiData.data())) {
      Logger::warn("DXGI: Failed to get XeSS version info");
      return XessVendorWa::Needed;
    }

    void* fiBlock = nullptr;
    UINT fiBlockSize = 0u;

    if (!VerQueryValueA(fiData.data(), "\\", &fiBlock, &fiBlockSize)) {
      Logger::warn("DXGI: Failed to get XeSS version info block");
      return XessVendorWa::Needed;
    }

    auto fiBlockTyped = reinterpret_cast<const VS_FIXEDFILEINFO*>(fiBlock);

    if (!fiBlockTyped || fiBlockSize < sizeof(*fiBlockTyped)) {
      Logger::warn("DXGI: Invalid XeSS version info block");
      return XessVendorWa::Needed;
    }

    // Some early 2.0 builds are still affected, keep the workaround enabled
    // until at least 2.0.2.68.
    bool isKnownGood = isVersionAtLeast(
      fiBlockTyped->dwProductVersionMS >> 16,
      fiBlockTyped->dwProductVersionMS & 0xffffu,
      fiBlockTyped->dwProductVersionLS >> 16,
      fiBlockTyped->dwProductVersionLS & 0xffffu,
      2u, 0u, 2u, 68u);

    return isKnownGood ? XessVendorWa::NotNeeded : XessVendorWa::Needed;
    #else
    return XessVendorWa::NotNeeded;
    #endif
  }

  static bool isNvapiEnabled() {
    return env::getEnvVar("DXVK_ENABLE_NVAPI") == "1";
  }


  bool DxgiOptions::shouldHideIntelGpu() const {
    if (this->hideIntelGpu)
      return true;

    XessVendorWa wa = m_xessVendorWa.load();

    // Keep checking until XeSS is loaded, then stick with the decision
    // so the reported vendor doesn't change again.
    if (wa == XessVendorWa::Unknown) {
      wa = getXessVendorWa();

      if (wa != XessVendorWa::Unknown) {
        XessVendorWa expected = XessVendorWa::Unknown;

        if (m_xessVendorWa.compare_exchange_strong(expected, wa)) {
          if (wa == XessVendorWa::Needed)
            Logger::info("DXGI: Detected XeSS usage, hiding Intel GPU Vendor");
        } else {
          wa = expected;
        }
      }
    }

    return wa == XessVendorWa::Needed;
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

    this->maxFrameRate     = config.getOption<int32_t>("dxvk.maxFrameRate",
                             config.getOption<int32_t>("dxgi.maxFrameRate", 0));
    this->syncInterval     = config.getOption<int32_t>("dxgi.syncInterval", -1);
    this->forceRefreshRate = config.getOption<int32_t>("dxgi.forceRefreshRate", 0u);

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

    this->enableHDR = config.getOption<bool>("dxgi.enableHDR", env::getEnvVar("DXVK_HDR") == "1");

    bool enableUe4Workarounds = config.getOption<bool>("dxgi.enableUe4Workarounds", false);

    if (this->enableHDR && isHDRDisallowed(enableUe4Workarounds)) {
      Logger::info("HDR was configured to be enabled, but has been force disabled as a UE4 DX11 game was detected.");
      this->enableHDR = false;
    }
  }

}
