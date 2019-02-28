#include "vulkan_loader.h"

#ifdef DXVK_NATIVE
  #define EXTERNAL_D3D_NO_VULKAN_H
  #include <external_d3d.h>
#endif

namespace dxvk::vk {

#if defined(__WINE__)

  extern "C"
  PFN_vkVoidFunction native_vkGetInstanceProcAddrWINE(VkInstance instance, const char *name);
  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = native_vkGetInstanceProcAddrWINE;

#elif defined(DXVK_NATIVE)
  // Set this later
  static PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
#else

  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = vkGetInstanceProcAddr;

#endif

  PFN_vkVoidFunction LibraryLoader::sym(const char* name) const {
#ifdef DXVK_NATIVE
    GetInstanceProcAddr = ::g_native_info.pfn_vkGetInstanceProcAddr;
#endif
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