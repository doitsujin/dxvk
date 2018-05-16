#include "dxvk_vulkan_extensions.h"

namespace dxvk::vk {
  
  void NameSet::add(const std::string& name) {
    m_names.insert(name);
  }
  
  
  bool NameSet::contains(const std::string& name) const {
    return m_names.find(name) != m_names.end();
  }
  
  
  NameSet NameSet::enumerateInstanceLayers(const LibraryFn& vkl) {
    uint32_t layerCount = 0;
    if (vkl.vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS)
      throw DxvkError(str::format("LayerSet::enumerateInstanceLayers: Failed to query instance layers"));
    
    std::vector<VkLayerProperties> layers(layerCount);
    if (vkl.vkEnumerateInstanceLayerProperties(&layerCount, layers.data()) != VK_SUCCESS)
      throw DxvkError(str::format("LayerSet::enumerateInstanceLayers: Failed to query instance layers"));
    
    NameSet result;
    for (const auto& layer : layers)
      result.m_names.insert(layer.layerName);
    return result;
  }
  
  
  NameSet NameSet::enumerateInstanceExtensions(
    const LibraryFn&        vkl,
    const NameList&         layers) {
    NameSet result;
    result.addInstanceLayerExtensions(vkl, nullptr);
    
    for (size_t i = 0; i < layers.count(); i++)
      result.addInstanceLayerExtensions(vkl, layers.name(i));
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
  
  
  void NameSet::addInstanceLayerExtensions(
    const LibraryFn&        vkl,
    const char*             layer) {
    uint32_t extCount = 0;
    if (vkl.vkEnumerateInstanceExtensionProperties(layer, &extCount, nullptr) != VK_SUCCESS)
      throw DxvkError("ExtensionSet::addInstanceExtensions: Failed to query instance extensions");
    
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkl.vkEnumerateInstanceExtensionProperties(layer, &extCount, extensions.data()) != VK_SUCCESS)
      throw DxvkError("ExtensionSet::addInstanceExtensions: Failed to query instance extensions");
    
    for (const auto& ext : extensions)
      this->add(ext.extensionName);
  }
  
}
