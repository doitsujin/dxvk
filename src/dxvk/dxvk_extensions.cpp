#include "dxvk_extensions.h"

namespace dxvk {
  
  DxvkNameSet::DxvkNameSet() { }
  DxvkNameSet::~DxvkNameSet() { }


  void DxvkNameSet::add(const char* pName) {
    m_names.insert({ pName, 1u });
  }


  void DxvkNameSet::merge(const DxvkNameSet& names) {
    for (const auto& pair : names.m_names)
      m_names.insert(pair);
  }


  uint32_t DxvkNameSet::supports(const char* pName) const {
    auto entry = m_names.find(pName);

    if (entry == m_names.end())
      return 0;
    
    return entry->second != 0
      ? entry->second
      : 1;
  }


  bool DxvkNameSet::enableExtensions(
          uint32_t          numExtensions,
          DxvkExt**         ppExtensions,
          DxvkNameSet&       nameSet) const {
    bool allRequiredEnabled = true;

    for (uint32_t i = 0; i < numExtensions; i++) {
      DxvkExt* ext = ppExtensions[i];

      if (ext->mode() == DxvkExtMode::Disabled)
        continue;
      
      uint32_t revision = supports(ext->name());

      if (revision) {
        if (ext->mode() != DxvkExtMode::Passive)
          nameSet.add(ext->name());

        ext->enable(revision);
      } else if (ext->mode() == DxvkExtMode::Required) {
        Logger::info(str::format("Required Vulkan extension ", ext->name(), " not supported"));
        allRequiredEnabled = false;
        continue;
      }
    }

    return allRequiredEnabled;
  }


  void DxvkNameSet::disableExtension(
          DxvkExt&          ext) {
    m_names.erase(ext.name());
    ext.disable();
  }


  DxvkNameList DxvkNameSet::toNameList() const {
    DxvkNameList nameList;
    for (const auto& pair : m_names)
      nameList.add(pair.first.c_str());
    return nameList;
  }


  DxvkNameSet DxvkNameSet::enumInstanceLayers(const Rc<vk::LibraryFn>& vkl) {
    uint32_t entryCount = 0;
    if (vkl->vkEnumerateInstanceLayerProperties(
          &entryCount, nullptr) != VK_SUCCESS)
      return DxvkNameSet();
    
    std::vector<VkLayerProperties> entries(entryCount);
    if (vkl->vkEnumerateInstanceLayerProperties(
          &entryCount, entries.data()) != VK_SUCCESS)
      return DxvkNameSet();

    DxvkNameSet set;
    for (uint32_t i = 0; i < entryCount; i++)
      set.m_names.insert({ entries[i].layerName, entries[i].specVersion });
    return set;
  }
  

  DxvkNameSet DxvkNameSet::enumInstanceExtensions(const Rc<vk::LibraryFn>& vkl) {
    uint32_t entryCount = 0;
    if (vkl->vkEnumerateInstanceExtensionProperties(
          nullptr, &entryCount, nullptr) != VK_SUCCESS)
      return DxvkNameSet();
    
    std::vector<VkExtensionProperties> entries(entryCount);
    if (vkl->vkEnumerateInstanceExtensionProperties(
          nullptr, &entryCount, entries.data()) != VK_SUCCESS)
      return DxvkNameSet();

    DxvkNameSet set;
    for (uint32_t i = 0; i < entryCount; i++)
      set.m_names.insert({ entries[i].extensionName, entries[i].specVersion });
    return set;
  }

  
  DxvkNameSet DxvkNameSet::enumDeviceExtensions(
    const Rc<vk::InstanceFn>& vki,
          VkPhysicalDevice    device) {
    uint32_t entryCount = 0;
    if (vki->vkEnumerateDeviceExtensionProperties(
          device, nullptr, &entryCount, nullptr) != VK_SUCCESS)
      return DxvkNameSet();
    
    std::vector<VkExtensionProperties> entries(entryCount);
    if (vki->vkEnumerateDeviceExtensionProperties(
          device, nullptr, &entryCount, entries.data()) != VK_SUCCESS)
      return DxvkNameSet();

    DxvkNameSet set;
    for (uint32_t i = 0; i < entryCount; i++)
      set.m_names.insert({ entries[i].extensionName, entries[i].specVersion });
    return set;
  }
  
}