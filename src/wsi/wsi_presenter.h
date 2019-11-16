#pragma once

#include <windows.h>

#include "../vulkan/vulkan_loader.h"

namespace dxvk::wsi {

  /**
    * \brief Create a surface for a window
    * 
    * \param [in] hWindow The window
    * \param [in] vki The instance
    * \param [out] pSurface The surface
    */
  VkResult createSurface(
          HWND                hWindow,
    const Rc<vk::InstanceFn>& vki,
          VkSurfaceKHR*       pSurface);

}