#if defined(DXVK_WSI_WIN32)

#include "wsi_platform_win32.h"

namespace dxvk::wsi {

  std::vector<const char *> Win32WsiDriver::getInstanceExtensions() {
    return { VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
  }

  static bool createWin32WsiDriver(WsiDriver **driver) {
    *driver = new Win32WsiDriver();
    return true;
  }

  WsiBootstrap Win32WSI = {
    "Win32",
    createWin32WsiDriver
  };

}

#endif
