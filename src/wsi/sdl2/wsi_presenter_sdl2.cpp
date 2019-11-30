#include "../wsi_presenter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <wsi/native_wsi.h>

namespace dxvk::wsi {

  VkResult createSurface(
          HWND                hWindow,
    const Rc<vk::InstanceFn>& vki,
          VkSurfaceKHR*       pSurface) {
    SDL_Window* window = fromHwnd(hWindow);

    return SDL_Vulkan_CreateSurface(window, vki->instance(), pSurface)
         ? VK_SUCCESS
         : VK_ERROR_OUT_OF_HOST_MEMORY;
  }

}