#include "../dxvk_platform_exts.h"

namespace dxvk {


  std::string_view DxvkPlatformExts::getName() {
    return "Win32 WSI";
  }


  DxvkNameSet DxvkPlatformExts::getInstanceExtensions() {
    DxvkNameSet names;
    names.add(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    return names;
  }


  DxvkNameSet DxvkPlatformExts::getDeviceExtensions(
          uint32_t      adapterId) {
    return DxvkNameSet();
  }
  

  void DxvkPlatformExts::initInstanceExtensions() {}


  void DxvkPlatformExts::initDeviceExtensions(
    const DxvkInstance* instance) {}


  DxvkPlatformExts g_platformInstance;

}