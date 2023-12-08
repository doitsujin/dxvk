#include "dxvk_platform_exts.h"
#include "../wsi/wsi_platform.h"

namespace dxvk {

  DxvkPlatformExts DxvkPlatformExts::s_instance;

  std::string_view DxvkPlatformExts::getName() {
    return "Platform WSI";
  }


  DxvkNameSet DxvkPlatformExts::getInstanceExtensions() {
    std::vector<const char *> extensionNames = wsi::getInstanceExtensions();

    DxvkNameSet names;
    for (const char* name : extensionNames)
      names.add(name);

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
