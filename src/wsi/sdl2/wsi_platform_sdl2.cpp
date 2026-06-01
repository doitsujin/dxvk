#if defined(DXVK_WSI_SDL2)

#include "wsi_platform_sdl2.h"
#include "../wsi_sonames.h"
#include "../../util/util_error.h"
#include "../../util/util_string.h"
#include "../../util/util_win32_compat.h"

#include <SDL_vulkan.h>

namespace dxvk::wsi {

  Sdl2WsiDriver::Sdl2WsiDriver() {
    libsdl = LoadLibraryA(WSI_SDL2_SONAME);
    if (libsdl == nullptr)
      throw DxvkError("SDL2 WSI: Failed to load SDL2 DLL.");

    #define SDL_PROC(ret, name, params) \
      name = reinterpret_cast<pfn_##name>(GetProcAddress(libsdl, #name)); \
      if (name == nullptr) { \
        FreeLibrary(libsdl); \
        libsdl = nullptr; \
        throw DxvkError("SDL2 WSI: Failed to load " #name "."); \
      }
    #include "wsi_platform_sdl2_funcs.h"
  }

  Sdl2WsiDriver::~Sdl2WsiDriver() {
    FreeLibrary(libsdl);
  }

  std::vector<const char *> Sdl2WsiDriver::getInstanceExtensions() {
    SDL_Vulkan_LoadLibrary(nullptr);

    uint32_t extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &extensionCount, nullptr))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extension count. ", SDL_GetError()));

    auto extensionNames = std::vector<const char *>(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &extensionCount, extensionNames.data()))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extensions. ", SDL_GetError()));

    return extensionNames;
  }

  static bool createSdl2WsiDriver(WsiDriver **driver) {
    try {
      *driver = new Sdl2WsiDriver();
    } catch (const DxvkError& e) {
      return false;
    }
    return true;
  }

  WsiBootstrap Sdl2WSI = {
    "SDL2",
    createSdl2WsiDriver
  };

}

#endif
