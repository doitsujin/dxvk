#include "dxvk_openvr.h"

#include <openvr/openvr.hpp>

namespace dxvk {
  
  VrInstance::VrInstance()
  : m_compositor(getCompositor()) {
    
  }
  
  
  VrInstance::~VrInstance() {
    
  }
  
  
  vk::NameSet VrInstance::queryInstanceExtensions() const {
    if (m_compositor != nullptr) {
      uint32_t len = m_compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
      std::vector<char> extensionList(len);
      len = m_compositor->GetVulkanInstanceExtensionsRequired(extensionList.data(), len);
      return parseExtensionList(std::string(extensionList.data(), len));
    } return vk::NameSet();
  }
  
  
  vk::NameSet VrInstance::queryDeviceExtensions(VkPhysicalDevice adapter) const {
    if (m_compositor != nullptr) {
      uint32_t len = m_compositor->GetVulkanDeviceExtensionsRequired(adapter, nullptr, 0);
      std::vector<char> extensionList(len);
      len = m_compositor->GetVulkanDeviceExtensionsRequired(adapter, extensionList.data(), len);
      return parseExtensionList(std::string(extensionList.data(), len));
    } return vk::NameSet();
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
      void* VR_CALLTYPE (*)(const char*, vr::EVRInitError*);
    
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