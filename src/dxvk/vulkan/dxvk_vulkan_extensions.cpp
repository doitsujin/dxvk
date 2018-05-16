#include "dxvk_vulkan_extensions.h"

namespace dxvk::vk {
  
  void NameSet::add(const std::string& name) {
    m_names.insert(name);
  }
  
  
  void NameSet::merge(const NameSet& other) {
    for (const auto& entry : other.m_names)
      this->add(entry);
  }
  
  
  bool NameSet::contains(const std::string& name) const {
    return m_names.find(name) != m_names.end();
  }
  
  
  NameSet NameSet::enumerateInstanceExtensions(
    const LibraryFn&        vkl) {
    uint32_t extCount = 0;
    if (vkl.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr) != VK_SUCCESS)
      throw DxvkError("ExtensionSet::addInstanceExtensions: Failed to query instance extensions");
    
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkl.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extensions.data()) != VK_SUCCESS)
      throw DxvkError("ExtensionSet::addInstanceExtensions: Failed to query instance extensions");
    
    NameSet result;
    for (const auto& ext : extensions)
      result.add(ext.extensionName);
    return result;
  }
  
  
  NameSet NameSet::enumerateDeviceExtensions(
    const InstanceFn&       vki,
          VkPhysicalDevice  device) {
    uint32_t extCount = 0;
    if (vki.vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr) != VK_SUCCESS)
      throw DxvkError("ExtensionSet::addDeviceExtensions: Failed to query device extensions");
    
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vki.vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, extensions.data()) != VK_SUCCESS)
      throw DxvkError("ExtensionSet::addDeviceExtensions: Failed to query device extensions");
    
    NameSet result;
    for (const auto& ext : extensions)
      result.add(ext.extensionName);
    return result;
  }
  
  
  NameList NameSet::getNameList() const {
    NameList result;
    for (const std::string& name : m_names)
      result.add(name.c_str());
    return result;
  }
  
}
