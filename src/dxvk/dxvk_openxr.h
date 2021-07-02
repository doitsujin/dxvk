#pragma once

#include <mutex>
#include <vector>

#include "dxvk_extension_provider.h"

namespace dxvk {

  class DxvkInstance;

  /**
   * \brief OpenXR instance
   * 
   * Loads OpenXR to provide access to Vulkan extension queries.
   */
  class DxvkXrProvider : public DxvkExtensionProvider {
    
  public:
    
    DxvkXrProvider();
    ~DxvkXrProvider();

    std::string_view getName();

    DxvkNameSet getInstanceExtensions();

    DxvkNameSet getDeviceExtensions(
            uint32_t      adapterId);
    
    void initInstanceExtensions();

    void initDeviceExtensions(
      const DxvkInstance* instance);

    static DxvkXrProvider s_instance;

  private:

    dxvk::mutex           m_mutex;
    HMODULE               m_wineOxr     = nullptr;

    bool m_loadedOxrApi      = false;
    bool m_initializedInsExt = false;
    bool m_initializedDevExt = false;

    DxvkNameSet              m_insExtensions;
    DxvkNameSet              m_devExtensions;
    
    DxvkNameSet queryInstanceExtensions() const;

    DxvkNameSet queryDeviceExtensions() const;

    DxvkNameSet parseExtensionList(
      const std::string&              str) const;
    
    bool loadFunctions();

    void shutdown();

    HMODULE loadLibrary();

    void freeLibrary();

    void* getSym(const char* sym);
    
  };
  
}
