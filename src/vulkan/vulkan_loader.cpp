#include "vulkan_loader.h"

#ifdef DXVK_NATIVE
  #define EXTERNAL_D3D_NO_VULKAN_H
  #include <external_d3d.h>
#endif

namespace dxvk::vk {

  LibraryLoader::LibraryLoader(PFN_vkGetInstanceProcAddr getInstanceProcAddr)
  : m_getInstanceProcAddr(getInstanceProcAddr) { }

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
    return m_getInstanceProcAddr(nullptr, name);
  }
  
  
  InstanceLoader::InstanceLoader(PFN_vkGetInstanceProcAddr getInstanceProcAddr, bool owned, VkInstance instance)
  : m_getInstanceProcAddr(getInstanceProcAddr), m_instance(instance), m_owned(owned) { }
  
  
  PFN_vkVoidFunction InstanceLoader::sym(const char* name) const {
    return m_getInstanceProcAddr(m_instance, name);
  }
  
  
  DeviceLoader::DeviceLoader(PFN_vkGetInstanceProcAddr getInstanceProcAddr, bool owned, VkInstance instance, VkDevice device)
  : m_getDeviceProcAddr(reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      getInstanceProcAddr(instance, "vkGetDeviceProcAddr"))),
    m_device(device), m_owned(owned) { }
  
  
  PFN_vkVoidFunction DeviceLoader::sym(const char* name) const {
    return m_getDeviceProcAddr(m_device, name);
  }
  
  
  LibraryFn::LibraryFn(PFN_vkGetInstanceProcAddr getInstanceProcAddr)
  : LibraryLoader(getInstanceProcAddr) { }
  LibraryFn::~LibraryFn() { }
  
  
  InstanceFn::InstanceFn(PFN_vkGetInstanceProcAddr getInstanceProcAddr, bool owned, VkInstance instance)
  : InstanceLoader(getInstanceProcAddr, owned, instance) { }
  InstanceFn::~InstanceFn() {
    if (m_owned)
      this->vkDestroyInstance(m_instance, nullptr);
  }
  
  
  DeviceFn::DeviceFn(PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, bool owned, VkInstance instance, VkDevice device)
  : DeviceLoader(vkGetInstanceProcAddr, owned, instance, device) { }
  DeviceFn::~DeviceFn() {
    if (m_owned)
      this->vkDestroyDevice(m_device, nullptr);
  }
  
}