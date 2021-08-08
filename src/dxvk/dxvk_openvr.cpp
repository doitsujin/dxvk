#include "dxvk_instance.h"
#include "dxvk_openvr.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <openvr/openvr.hpp>

using VR_InitInternalProc        = vr::IVRSystem* (VR_CALLTYPE *)(vr::EVRInitError*, vr::EVRApplicationType);
using VR_ShutdownInternalProc    = void  (VR_CALLTYPE *)();
using VR_GetGenericInterfaceProc = void* (VR_CALLTYPE *)(const char*, vr::EVRInitError*);

namespace dxvk {
  
  struct VrFunctions {
    VR_InitInternalProc        initInternal        = nullptr;
    VR_ShutdownInternalProc    shutdownInternal    = nullptr;
    VR_GetGenericInterfaceProc getGenericInterface = nullptr;
  };
  
  VrFunctions g_vrFunctions;
  VrInstance VrInstance::s_instance;

  VrInstance:: VrInstance() {
    m_no_vr = env::getEnvVar("DXVK_NO_VR") == "1";
  }
  VrInstance::~VrInstance() { }


  std::string_view VrInstance::getName() {
    return "OpenVR";
  }
  
  
  DxvkNameSet VrInstance::getInstanceExtensions() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return m_insExtensions;
  }


  DxvkNameSet VrInstance::getDeviceExtensions(uint32_t adapterId) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    if (adapterId < m_devExtensions.size())
      return m_devExtensions[adapterId];
    
    return DxvkNameSet();
  }


  void VrInstance::initInstanceExtensions() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_no_vr || m_initializedDevExt)
        return;

    if (!m_vr_key)
    {
        LSTATUS status;

        if ((status = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Wine\\VR", 0, KEY_READ, &m_vr_key)))
            Logger::info(str::format("OpenVR: could not open registry key, status ", status));
    }

    if (!m_vr_key && !m_compositor)
      m_compositor = this->getCompositor();

    if (!m_vr_key && !m_compositor)
      return;
    
    m_insExtensions = this->queryInstanceExtensions();
    m_initializedInsExt = true;
  }


  void VrInstance::initDeviceExtensions(const DxvkInstance* instance) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_no_vr || (!m_vr_key && !m_compositor) || m_initializedDevExt)
      return;
    
    for (uint32_t i = 0; instance->enumAdapters(i) != nullptr; i++) {
      m_devExtensions.push_back(this->queryDeviceExtensions(
        instance->enumAdapters(i)));
    }

    m_initializedDevExt = true;
    this->shutdown();
  }

  bool VrInstance::waitVrKeyReady() const {
    DWORD type, value, wait_status, size;
    LSTATUS status;
    HANDLE event;

    size = sizeof(value);
    if ((status = RegQueryValueExA(m_vr_key, "state", nullptr, &type, reinterpret_cast<BYTE*>(&value), &size)))
    {
        Logger::err(str::format("OpenVR: could not query value, status ", status));
        return false;
    }
    if (type != REG_DWORD)
    {
        Logger::err(str::format("OpenVR: unexpected value type ", type));
        return false;
    }

    if (value)
        return value == 1;

    event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    while (1)
    {
        if (RegNotifyChangeKeyValue(m_vr_key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, event, TRUE))
        {
            Logger::err("Error registering registry change notification");
            goto done;
        }
        size = sizeof(value);
        if ((status = RegQueryValueExA(m_vr_key, "state", nullptr, &type, reinterpret_cast<BYTE*>(&value), &size)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            goto done;
        }
        if (value)
            break;
        while ((wait_status = WaitForSingleObject(event, 1000)) == WAIT_TIMEOUT)
            Logger::warn("VR state wait timeout (retrying)");

        if (wait_status != WAIT_OBJECT_0)
        {
            Logger::err(str::format("Got unexpected wait status ", wait_status));
            break;
        }
    }

  done:
    CloseHandle(event);
    return value == 1;
  }

  DxvkNameSet VrInstance::queryInstanceExtensions() const {
    std::vector<char> extensionList;
    DWORD len;

    if (m_vr_key)
    {
        LSTATUS status;
        DWORD type;

        if (!this->waitVrKeyReady())
            return DxvkNameSet();

        len = 0;
        if ((status = RegQueryValueExA(m_vr_key, "openvr_vulkan_instance_extensions", nullptr, &type, nullptr, &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
        extensionList.resize(len);
        if ((status = RegQueryValueExA(m_vr_key, "openvr_vulkan_instance_extensions", nullptr, &type, reinterpret_cast<BYTE*>(extensionList.data()), &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
    }
    else
    {
        len = m_compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
        extensionList.resize(len);
        len = m_compositor->GetVulkanInstanceExtensionsRequired(extensionList.data(), len);
    }
    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  DxvkNameSet VrInstance::queryDeviceExtensions(Rc<DxvkAdapter> adapter) const {
    std::vector<char> extensionList;
    DWORD len;

    if (m_vr_key)
    {
        LSTATUS status;
        char name[256];
        DWORD type;

        if (!this->waitVrKeyReady())
            return DxvkNameSet();

        sprintf(name, "PCIID:%04x:%04x", adapter->deviceProperties().vendorID, adapter->deviceProperties().deviceID);
        len = 0;
        if ((status = RegQueryValueExA(m_vr_key, name, nullptr, &type, nullptr, &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
        extensionList.resize(len);
        if ((status = RegQueryValueExA(m_vr_key, name, nullptr, &type, reinterpret_cast<BYTE*>(extensionList.data()), &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
    }
    else
    {
        len = m_compositor->GetVulkanDeviceExtensionsRequired(adapter->handle(), nullptr, 0);
        extensionList.resize(len);
        len = m_compositor->GetVulkanDeviceExtensionsRequired(adapter->handle(), extensionList.data(), len);
    }
    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  DxvkNameSet VrInstance::parseExtensionList(const std::string& str) const {
    DxvkNameSet result;
    
    std::stringstream strstream(str);
    std::string       section;
    
    while (std::getline(strstream, section, ' '))
      result.add(section.c_str());
    
    return result;
  }
  
  
  vr::IVRCompositor* VrInstance::getCompositor() {
    // Skip OpenVR initialization if requested
    
    // Locate the OpenVR DLL if loaded by the process. Some
    // applications may not have OpenVR loaded at the time
    // they create the DXGI instance, so we try our own DLL.
    m_ovrApi = this->loadLibrary();
    
    if (!m_ovrApi) {
      Logger::info("OpenVR: Failed to locate module");
      return nullptr;
    }
    
    // Load method used to retrieve the IVRCompositor interface
    g_vrFunctions.initInternal        = reinterpret_cast<VR_InitInternalProc>       (this->getSym("VR_InitInternal"));
    g_vrFunctions.shutdownInternal    = reinterpret_cast<VR_ShutdownInternalProc>   (this->getSym("VR_ShutdownInternal"));
    g_vrFunctions.getGenericInterface = reinterpret_cast<VR_GetGenericInterfaceProc>(this->getSym("VR_GetGenericInterface"));
    
    if (!g_vrFunctions.getGenericInterface) {
      Logger::warn("OpenVR: VR_GetGenericInterface not found");
      return nullptr;
    }
    
    // Retrieve the compositor interface
    vr::EVRInitError error = vr::VRInitError_None;
    
    vr::IVRCompositor* compositor = reinterpret_cast<vr::IVRCompositor*>(
      g_vrFunctions.getGenericInterface(vr::IVRCompositor_Version, &error));
    
    if (error != vr::VRInitError_None || !compositor) {
      if (!g_vrFunctions.initInternal
       || !g_vrFunctions.shutdownInternal) {
        Logger::warn("OpenVR: VR_InitInternal or VR_ShutdownInternal not found");
        return nullptr;
      }

      // If the app has not initialized OpenVR yet, we need
      // to do it now in order to grab a compositor instance
      g_vrFunctions.initInternal(&error, vr::VRApplication_Background);
      m_initializedOpenVr = error == vr::VRInitError_None;

      if (error != vr::VRInitError_None) {
        Logger::warn("OpenVR: Failed to initialize OpenVR");
        return nullptr;
      }

      compositor = reinterpret_cast<vr::IVRCompositor*>(
        g_vrFunctions.getGenericInterface(vr::IVRCompositor_Version, &error));
      
      if (error != vr::VRInitError_None || !compositor) {
        Logger::warn("OpenVR: Failed to query compositor interface");
        this->shutdown();
        return nullptr;
      }
    }
    
    Logger::info("OpenVR: Compositor interface found");
    return compositor;
  }


  void VrInstance::shutdown() {
    if (m_vr_key)
    {
        RegCloseKey(m_vr_key);
        m_vr_key = nullptr;
    }

    if (m_initializedOpenVr)
      g_vrFunctions.shutdownInternal();

    if (m_loadedOvrApi)
      this->freeLibrary();
    
    m_initializedOpenVr = false;
    m_loadedOvrApi      = false;
  }


  HMODULE VrInstance::loadLibrary() {
    HMODULE handle = nullptr;
    if (!(handle = ::GetModuleHandle("openvr_api.dll"))) {
      handle = ::LoadLibrary("openvr_api_dxvk.dll");
      m_loadedOvrApi = handle != nullptr;
    }
    return handle;
  }


  void VrInstance::freeLibrary() {
    ::FreeLibrary(m_ovrApi);
  }

  
  void* VrInstance::getSym(const char* sym) {
    return reinterpret_cast<void*>(
      ::GetProcAddress(m_ovrApi, sym));
  }
  
}
