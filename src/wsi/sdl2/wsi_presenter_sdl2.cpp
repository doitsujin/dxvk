#include "../wsi_presenter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace dxvk::wsi {

  VkResult createSurface(
          HWND                hWindow,
    const Rc<vk::InstanceFn>& vki,
          VkSurfaceKHR*       pSurface) {
    return SDL_Vulkan_CreateSurface(hWindow, vki->instance(), pSurface)
         ? VK_SUCCESS
         : VK_ERROR_OUT_OF_HOST_MEMORY;
  }

}