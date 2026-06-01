#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>
#include <vector>

#include "vulkan_loader.h"

#include "../util/log/log.h"

#include "../util/util_string.h"
#include "../util/util_win32_compat.h"

namespace dxvk::vk {

  static std::vector<std::string> getVulkanLibraryCandidates() {
    static const std::array<const char*, 2> dllNames = {{
#ifdef _WIN32
      "winevulkan.dll",
      "vulkan-1.dll",
#elif defined(__APPLE__)
      "libvulkan.dylib",
      "libMoltenVK.dylib",
#else
      "libvulkan.so",
      "libvulkan.so.1",
#endif
    }};

    std::vector<std::string> candidates;

    // Try the bare soname first so an explicit DYLD_LIBRARY_PATH / LD_LIBRARY_PATH
    // or @rpath entry (if the loader was configured with one) takes precedence.
    for (auto dllName : dllNames)
      candidates.emplace_back(dllName);

#if defined(__APPLE__)
    // Homebrew installs libvulkan.dylib (vulkan-loader) and libMoltenVK.dylib
    // (molten-vk) into <prefix>/lib, which is not on dyld's default search path.
    // Without this fallback the library only loads when the host app exports
    // DYLD_LIBRARY_PATH. Probe the common Homebrew prefixes so MoltenVK is found
    // out of the box on both Apple Silicon and Intel Macs.
    std::vector<std::string> prefixes;

    if (const char* brewPrefix = std::getenv("HOMEBREW_PREFIX"))
      prefixes.emplace_back(brewPrefix);

    prefixes.emplace_back("/opt/homebrew"); // Apple Silicon default
    prefixes.emplace_back("/usr/local");    // Intel default

    for (const auto& prefix : prefixes) {
      for (auto dllName : dllNames)
        candidates.emplace_back(str::format(prefix, "/lib/", dllName));
    }
#endif

    return candidates;
  }

#if defined(__APPLE__)
  static bool fileExists(const std::string& path) {
    if (FILE* file = std::fopen(path.c_str(), "r")) {
      std::fclose(file);
      return true;
    }
    return false;
  }

  // When using the full Vulkan loader (libvulkan.dylib) the MoltenVK ICD is
  // discovered via a manifest in <prefix>/share/vulkan/icd.d. Homebrew's prefix
  // is not on the loader's default manifest search path, so point it at the
  // MoltenVK manifest if the user has not already selected an ICD. Loading
  // libMoltenVK.dylib directly ignores this and is unaffected.
  static void setupMoltenVkIcd() {
    if (std::getenv("VK_ICD_FILENAMES") || std::getenv("VK_DRIVER_FILES"))
      return;

    std::vector<std::string> prefixes;

    if (const char* brewPrefix = std::getenv("HOMEBREW_PREFIX"))
      prefixes.emplace_back(brewPrefix);

    prefixes.emplace_back("/opt/homebrew");
    prefixes.emplace_back("/usr/local");

    for (const auto& prefix : prefixes) {
      std::string manifest = str::format(prefix, "/share/vulkan/icd.d/MoltenVK_icd.json");

      if (fileExists(manifest)) {
        setenv("VK_ICD_FILENAMES", manifest.c_str(), 0);
        Logger::info(str::format("Vulkan: Using MoltenVK ICD manifest ", manifest));
        return;
      }
    }
  }
#endif

  static std::pair<HMODULE, PFN_vkGetInstanceProcAddr> loadVulkanLibrary() {
#if defined(__APPLE__)
    setupMoltenVkIcd();
#endif

    for (const auto& dllName : getVulkanLibraryCandidates()) {
      HMODULE library = LoadLibraryA(dllName.c_str());

      if (!library)
        continue;

      auto proc = GetProcAddress(library, "vkGetInstanceProcAddr");

      if (!proc) {
        FreeLibrary(library);
        continue;
      }

      Logger::info(str::format("Vulkan: Found vkGetInstanceProcAddr in ", dllName, " @ 0x", std::hex, reinterpret_cast<uintptr_t>(proc)));
      return std::make_pair(library, reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc));
    }

    Logger::err("Vulkan: vkGetInstanceProcAddr not found");
    return { };
  }

  LibraryLoader::LibraryLoader() {
    std::tie(m_library, m_getInstanceProcAddr) = loadVulkanLibrary();
  }

  LibraryLoader::LibraryLoader(PFN_vkGetInstanceProcAddr loaderProc) {
    m_getInstanceProcAddr = loaderProc;
  }

  LibraryLoader::~LibraryLoader() {
    if (m_library)
      FreeLibrary(m_library);
  }

  PFN_vkVoidFunction LibraryLoader::sym(VkInstance instance, const char* name) const {
    return m_getInstanceProcAddr(instance, name);
  }

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
    return sym(nullptr, name);
  }

  
  InstanceLoader::InstanceLoader(const Rc<LibraryLoader>& library, bool owned, VkInstance instance)
  : m_library(library), m_instance(instance), m_owned(owned) { }
  
  
  PFN_vkVoidFunction InstanceLoader::sym(const char* name) const {
    return m_library->sym(m_instance, name);
  }
  
  
  DeviceLoader::DeviceLoader(const Rc<InstanceLoader>& library, bool owned, VkDevice device)
  : m_library(library)
  , m_getDeviceProcAddr(reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      m_library->sym("vkGetDeviceProcAddr"))),
    m_device(device), m_owned(owned) { }
  
  
  PFN_vkVoidFunction DeviceLoader::sym(const char* name) const {
    return m_getDeviceProcAddr(m_device, name);
  }
  
  
  LibraryFn::LibraryFn() { }
  LibraryFn::LibraryFn(PFN_vkGetInstanceProcAddr loaderProc)
  : LibraryLoader(loaderProc) { }
  LibraryFn::~LibraryFn() { }
  
  
  InstanceFn::InstanceFn(const Rc<LibraryLoader>& library, bool owned, VkInstance instance)
  : InstanceLoader(library, owned, instance) { }
  InstanceFn::~InstanceFn() {
    if (m_owned)
      this->vkDestroyInstance(m_instance, nullptr);
  }
  
  
  DeviceFn::DeviceFn(const Rc<InstanceLoader>& library, bool owned, VkDevice device)
  : DeviceLoader(library, owned, device) { }
  DeviceFn::~DeviceFn() {
    if (m_owned)
      this->vkDestroyDevice(m_device, nullptr);
  }
  
}
