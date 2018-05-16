#include "dxvk_extensions.h"

namespace dxvk {
  
  DxvkExtensionList:: DxvkExtensionList() { }
  DxvkExtensionList::~DxvkExtensionList() { }
  
  
  void DxvkExtensionList::enableExtensions(const vk::NameSet& extensions) {
    for (auto ext : m_extensions) {
      if (extensions.contains(ext->name()))
        ext->setEnabled(true);
    }
  }
  
  
  bool DxvkExtensionList::checkSupportStatus() {
    bool requiredExtensionsEnabled = true;
    
    for (auto ext : m_extensions) {
      if (!ext->enabled()) {
        switch (ext->type()) {
          case DxvkExtensionType::Optional:
            // An optional extension should not have any impact on
            // the functionality of an application, so inform the
            // user only if verbose debug messages are enabled
            Logger::debug(str::format("Optional Vulkan extension ", ext->name(), " not supported"));
            break;
            
          case DxvkExtensionType::Desired:
            // If a desired extension is not supported, applications
            // should keep working but may exhibit unexpected behaviour,
            // so we'll inform the user anyway
            Logger::warn(str::format("Vulkan extension ", ext->name(), " not supported"));
            break;
            
          case DxvkExtensionType::Required:
            // Do not exit early so we can catch all unsupported extensions.
            requiredExtensionsEnabled = false;
            Logger::err(str::format("Required Vulkan extension ", ext->name(), " not supported"));
            break;
        }
      }
    }
    
    return requiredExtensionsEnabled;
  }
  
  
  vk::NameList DxvkExtensionList::getEnabledExtensionNames() const {
    vk::NameList names;
    
    for (auto ext : m_extensions) {
      if (ext->enabled())
        names.add(ext->name());
    }
    
    return names;
  }
  
  
  void DxvkExtensionList::registerExtension(DxvkExtension* extension) {
    m_extensions.push_back(extension);
  }
  
  
  DxvkExtension::DxvkExtension(
          DxvkExtensionList*  parent,
    const char*               name,
          DxvkExtensionType   type)
  : m_name(name), m_type(type), m_enabled(false) {
    parent->registerExtension(this);
  }
  
}