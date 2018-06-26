#include "dxvk_instance.h"
#include "dxvk_openvr.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <openvr/openvr.hpp>

namespace dxvk {
  
  VrInstance g_vrInstance;

  VrInstance:: VrInstance() { }
  VrInstance::~VrInstance() { }
  
  
  vk::NameSet VrInstance::getInstanceExtensions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_insExtensions;
  }


  vk::NameSet VrInstance::getDeviceExtensions(uint32_t adapterId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (adapterId < m_devExtensions.size())
      return m_devExtensions[adapterId];
    
    return vk::NameSet();
  }


  void VrInstance::initInstanceExtensions() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initializedInsExt)
      return;
    
    vr::IVRCompositor* compositor = this->getCompositor();

    if (compositor == nullptr)
      return;
    
    m_insExtensions = this->queryInstanceExtensions(compositor);
    m_initializedInsExt = true;
  }


  void VrInstance::initDeviceExtensions(const DxvkInstance* instance) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initializedDevExt)
      return;
    
    vr::IVRCompositor* compositor = this->getCompositor();

    if (compositor == nullptr)
      return;
    
    for (uint32_t i = 0; instance->enumAdapters(i) != nullptr; i++) {
      m_devExtensions.push_back(this->queryDeviceExtensions(
        compositor, instance->enumAdapters(i)->handle()));
    }

    m_initializedDevExt = true;
  }


  vk::NameSet VrInstance::queryInstanceExtensions(vr::IVRCompositor* compositor) const {
    uint32_t len = compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
    std::vector<char> extensionList(len);
    len = compositor->GetVulkanInstanceExtensionsRequired(extensionList.data(), len);
    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  vk::NameSet VrInstance::queryDeviceExtensions(vr::IVRCompositor* compositor, VkPhysicalDevice adapter) const {
    uint32_t len = compositor->GetVulkanDeviceExtensionsRequired(adapter, nullptr, 0);
    std::vector<char> extensionList(len);
    len = compositor->GetVulkanDeviceExtensionsRequired(adapter, extensionList.data(), len);
    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  vk::NameSet VrInstance::parseExtensionList(const std::string& str) {
    vk::NameSet result;
    
    std::stringstream strstream(str);
    std::string       section;
    
    while (std::getline(strstream, section, ' '))
      result.add(section);
    
    return result;
  }
  
  
  vr::IVRCompositor* VrInstance::getCompositor() {
    using GetGenericInterfaceProc = 
      void* (VR_CALLTYPE *)(const char*, vr::EVRInitError*);
    
    // Locate the OpenVR DLL if loaded by the process
    HMODULE ovrApi = ::GetModuleHandle("openvr_api.dll");
    
    if (ovrApi == nullptr) {
      Logger::warn("OpenVR: Failed to locate module");
      return nullptr;
    }
    
    // Load method used to retrieve the IVRCompositor interface
    auto vrGetGenericInterface = reinterpret_cast<GetGenericInterfaceProc>(
      ::GetProcAddress(ovrApi, "VR_GetGenericInterface"));
    
    if (vrGetGenericInterface == nullptr) {
      Logger::warn("OpenVR: VR_GetGenericInterface not found");
      return nullptr;
    }
    
    // Retrieve the compositor interface
    vr::EVRInitError error = vr::VRInitError_None;
    
    auto compositor = reinterpret_cast<vr::IVRCompositor*>(
      vrGetGenericInterface(vr::IVRCompositor_Version, &error));
    
    if (error != vr::VRInitError_None) {
      Logger::warn(str::format("OpenVR: Failed to retrieve ", vr::IVRCompositor_Version));
      return nullptr;
    }
    
    Logger::warn("OpenVR: Compositor interface found");
    return compositor;
  }
  
}