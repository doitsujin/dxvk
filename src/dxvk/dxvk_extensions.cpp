#include "dxvk_extensions.h"

namespace dxvk {
  
  DxvkNameSet::DxvkNameSet() { }
  DxvkNameSet::~DxvkNameSet() { }


  void DxvkNameSet::add(const char* pName) {
    m_names.insert(pName);
  }


  void DxvkNameSet::merge(const DxvkNameSet& names) {
    for (const std::string& name : names.m_names)
      m_names.insert(name);
  }


  bool DxvkNameSet::supports(const char* pName) const {
    return m_names.find(pName) != m_names.end();
  }


  bool DxvkNameSet::enableExtensions(
          uint32_t          numExtensions,
          DxvkExt**         ppExtensions,
          DxvkNameSet&       nameSet) const {
    bool allRequiredEnabled = true;

    for (uint32_t i = 0; i < numExtensions; i++) {
      DxvkExt* ext = ppExtensions[i];

      if (ext->mode() != DxvkExtMode::Disabled) {
        bool supported = supports(ext->name());

        if (supported) {
          nameSet.add(ext->name());
          ext->enable();
        } else if (ext->mode() == DxvkExtMode::Required) {
          Logger::info(str::format(
            "Required Vulkan extension ", ext->name(), " not supported"));
          allRequiredEnabled = false;
        }
      }
    }

    return allRequiredEnabled;
  }


  DxvkNameList DxvkNameSet::toNameList() const {
    DxvkNameList nameList;
    for (const std::string& name : m_names)
      nameList.add(name.c_str());
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
      set.m_names.insert(entries[i].layerName);
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
      set.m_names.insert(entries[i].extensionName);
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
      set.m_names.insert(entries[i].extensionName);
    return set;
  }
  
}