#include "dxvk_extension_provider.h"

#include "dxvk_openvr.h"
#include "dxvk_platform_exts.h"

namespace dxvk {

  DxvkExtensionProviderList DxvkExtensionProvider::s_extensionProviders = {
    &g_platformInstance,
    &g_vrInstance
  };

  const DxvkExtensionProviderList& DxvkExtensionProvider::getExtensionProviders() {
    return s_extensionProviders;
  }

}