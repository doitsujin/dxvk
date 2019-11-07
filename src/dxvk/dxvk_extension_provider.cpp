#include "dxvk_extension_provider.h"

#include "dxvk_openvr.h"
#include "dxvk_platform_exts.h"

namespace dxvk {

  DxvkExtensionProviderList DxvkExtensionProvider::s_extensionProviders = {
    &DxvkPlatformExts::s_instance,
    &VrInstance::s_instance,
  };

  const DxvkExtensionProviderList& DxvkExtensionProvider::getExtensionProviders() {
    return s_extensionProviders;
  }

}