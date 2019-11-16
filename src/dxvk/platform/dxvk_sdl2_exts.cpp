#include "../dxvk_platform_exts.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace dxvk {

  DxvkPlatformExts DxvkPlatformExts::s_instance;

  std::string_view DxvkPlatformExts::getName() {
    return "SDL2 WSI";
  }


  DxvkNameSet DxvkPlatformExts::getInstanceExtensions() {
    SDL_Window* window = SDL_CreateWindow(
      "Dummy Window",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      1, 1,
      SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);

    if (window == nullptr)
      throw DxvkError(str::format("SDL2 WSI: Failed to create dummy window. ", SDL_GetError()));

    uint32_t extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extension count. ", SDL_GetError()));

    auto extensionNames = std::vector<const char *>(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data()))
      throw DxvkError(str::format("SDL2 WSI: Failed to get instance extensions. ", SDL_GetError()));

    DxvkNameSet names;
    for (const char* name : extensionNames)
      names.add(name);

    SDL_DestroyWindow(window);

    return names;
  }


  DxvkNameSet DxvkPlatformExts::getDeviceExtensions(
          uint32_t      adapterId) {
    return DxvkNameSet();
  }
  

  void DxvkPlatformExts::initInstanceExtensions() {

  }


  void DxvkPlatformExts::initDeviceExtensions(
    const DxvkInstance* instance) {

  }

}