#include "wsi_platform_win32.h"

namespace dxvk::wsi {

  std::vector<const char *> getInstanceExtensions() {
    return { VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
  }

}
