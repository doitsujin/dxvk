#include "vulkan_loader.h"

namespace dxvk::vk {

  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = vkGetInstanceProcAddr;

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
    return dxvk::vk::GetInstanceProcAddr(nullptr, name);
  }
  
  
  InstanceLoader::InstanceLoader(bool owned, VkInstance instance)
  : m_instance(instance), m_owned(owned) { }
  
  
  PFN_vkVoidFunction InstanceLoader::sym(const char* name) const {
    return dxvk::vk::GetInstanceProcAddr(m_instance, name);
  }
  
  
  DeviceLoader::DeviceLoader(bool owned, VkInstance instance, VkDevice device)
  : m_getDeviceProcAddr(reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      dxvk::vk::GetInstanceProcAddr(instance, "vkGetDeviceProcAddr"))),
    m_device(device), m_owned(owned) { }
  
  
  PFN_vkVoidFunction DeviceLoader::sym(const char* name) const {
    return m_getDeviceProcAddr(m_device, name);
  }
  
  
  LibraryFn::LibraryFn() { }
  LibraryFn::~LibraryFn() { }
  
  
  InstanceFn::InstanceFn(bool owned, VkInstance instance)
  : InstanceLoader(owned, instance) { }
  InstanceFn::~InstanceFn() {
    if (m_owned)
      this->vkDestroyInstance(m_instance, nullptr);
  }
  
  
  DeviceFn::DeviceFn(bool owned, VkInstance instance, VkDevice device)
  : DeviceLoader(owned, instance, device) { }
  DeviceFn::~DeviceFn() {
    if (m_owned)
      this->vkDestroyDevice(m_device, nullptr);
  }
  
}