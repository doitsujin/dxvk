#include "dxvk_platform_exts.h"
#include "../wsi/wsi_platform.h"

namespace dxvk {

  DxvkPlatformExts DxvkPlatformExts::s_instance;

  std::string_view DxvkPlatformExts::getName() {
    return "Platform WSI";
  }


  DxvkExtensionList DxvkPlatformExts::getInstanceExtensions() {
    std::vector<const char *> extensionNames = wsi::getInstanceExtensions();

    DxvkExtensionList names;
    for (const char* name : extensionNames)
      names.push_back(vk::makeExtension(name));

    return names;
  }


  DxvkExtensionList DxvkPlatformExts::getDeviceExtensions(
          uint32_t      adapterId) {
    return DxvkExtensionList();
  }


  void DxvkPlatformExts::initInstanceExtensions() {

  }


  void DxvkPlatformExts::initDeviceExtensions(
    const DxvkInstance* instance) {

  }

}
