#include "dxvk_vulkan_loader.h"

namespace dxvk::vk {

#if defined(__WINE__)

  extern "C"
  PFN_vkVoidFunction native_vkGetInstanceProcAddrWINE(VkInstance instance, const char *name);
  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = native_vkGetInstanceProcAddrWINE;

#else

  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = vkGetInstanceProcAddr;

#endif

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
    return dxvk::vk::GetInstanceProcAddr(nullptr, name);
  }
  
  
  InstanceLoader::InstanceLoader(VkInstance instance)
  : m_instance(instance) { }
  
  
  PFN_vkVoidFunction InstanceLoader::sym(const char* name) const {
    return dxvk::vk::GetInstanceProcAddr(m_instance, name);
  }
  
  
  DeviceLoader::DeviceLoader(VkInstance instance, VkDevice device)
  : m_getDeviceProcAddr(reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      dxvk::vk::GetInstanceProcAddr(instance, "vkGetDeviceProcAddr"))),
    m_device(device) { }
  
  
  PFN_vkVoidFunction DeviceLoader::sym(const char* name) const {
    return m_getDeviceProcAddr(m_device, name);
  }
  
  
  LibraryFn::LibraryFn() { }
  LibraryFn::~LibraryFn() { }
  
  
  InstanceFn::InstanceFn(VkInstance instance)
  : InstanceLoader(instance) { }
  InstanceFn::~InstanceFn() {
    this->vkDestroyInstance(m_instance, nullptr);
  }
  
  
  DeviceFn::DeviceFn(VkInstance instance, VkDevice device)
  : DeviceLoader(instance, device) { }
  DeviceFn::~DeviceFn() {
    this->vkDestroyDevice(m_device, nullptr);
  }
  
}