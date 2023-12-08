#include "wsi_platform_sdl2.h"
#include "../../util/util_error.h"
#include "../../util/util_string.h"

#include <SDL2/SDL_vulkan.h>

namespace dxvk::wsi {

  std::vector<const char *> getInstanceExtensions() {
    SDL_Vulkan_LoadLibrary(nullptr);

    uint32_t extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &extensionCount, nullptr))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extension count. ", SDL_GetError()));

    auto extensionNames = std::vector<const char *>(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &extensionCount, extensionNames.data()))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extensions. ", SDL_GetError()));

    return extensionNames;
  }

}
