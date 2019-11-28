#pragma once

#include "dxvk_extension_provider.h"

namespace dxvk {

  class DxvkPlatformExts : public DxvkExtensionProvider {

  public:

    std::string_view getName();

    DxvkNameSet getInstanceExtensions();

    DxvkNameSet getDeviceExtensions(
            uint32_t      adapterId);
    
    void initInstanceExtensions();

    void initDeviceExtensions(
      const DxvkInstance* instance);

    static DxvkPlatformExts s_instance;
  };

}
