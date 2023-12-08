#include "wsi_platform_win32.h"

namespace dxvk::wsi {

  std::vector<const char *> Win32WsiDriver::getInstanceExtensions() {
    return { VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
  }

  WsiDriver* platformCreateWsiDriver() {
    return new Win32WsiDriver();
  }

}
