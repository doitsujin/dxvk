#include "../wsi_presenter.h"

namespace dxvk::wsi {

  VkResult createSurface(
          HWND                hWindow,
    const Rc<vk::InstanceFn>& vki,
          VkSurfaceKHR*       pSurface) {
    HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(hWindow, GWLP_HINSTANCE));

    VkWin32SurfaceCreateInfoKHR info;
    info.sType      = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.pNext      = nullptr;
    info.flags      = 0;
    info.hinstance  = hInstance;
    info.hwnd       = hWindow;
    
    return vki->vkCreateWin32SurfaceKHR(
      vki->instance(), &info, nullptr, pSurface);
  }

}