#pragma once

#include <mutex>
#include <vector>

#include "dxvk_extension_provider.h"

#ifdef __WINE__
using SoHandle = void*;
#else
using SoHandle = HMODULE;
#endif

namespace vr {
  class IVRCompositor;
  class IVRSystem;
}

namespace dxvk {

  class DxvkInstance;

  /**
   * \brief OpenVR instance
   * 
   * Loads Initializes OpenVR to provide
   * access to Vulkan extension queries.
   */
  class VrInstance : public DxvkExtensionProvider {
    
  public:
    
    VrInstance();
    ~VrInstance();

    std::string_view getName();

    DxvkNameSet getInstanceExtensions();

    DxvkNameSet getDeviceExtensions(
            uint32_t      adapterId);
    
    void initInstanceExtensions();

    void initDeviceExtensions(
      const DxvkInstance* instance);

    static VrInstance s_instance;

  private:

    std::mutex            m_mutex;
    vr::IVRCompositor*    m_compositor = nullptr;
    SoHandle              m_ovrApi     = nullptr;

    bool m_loadedOvrApi      = false;
    bool m_initializedOpenVr = false;
    bool m_initializedInsExt = false;
    bool m_initializedDevExt = false;

    DxvkNameSet              m_insExtensions;
    std::vector<DxvkNameSet> m_devExtensions;
    
    DxvkNameSet queryInstanceExtensions() const;

    DxvkNameSet queryDeviceExtensions(
            VkPhysicalDevice          adapter) const;

    DxvkNameSet parseExtensionList(
      const std::string&              str) const;
    
    vr::IVRCompositor* getCompositor();

    void shutdown();

    SoHandle loadLibrary();

    void freeLibrary();

    void* getSym(const char* sym);
    
  };

  extern VrInstance g_vrInstance;
  
}