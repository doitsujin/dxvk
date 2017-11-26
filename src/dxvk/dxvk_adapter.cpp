#include <cstring>

#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxvk_instance.h"
#include "dxvk_surface.h"

namespace dxvk {
  
  DxvkAdapter::DxvkAdapter(
    const Rc<DxvkInstance>&   instance,
          VkPhysicalDevice    handle)
  : m_instance      (instance),
    m_vki           (instance->vki()),
    m_handle        (handle) {
    uint32_t numQueueFamilies = 0;
    m_vki->vkGetPhysicalDeviceQueueFamilyProperties(
      m_handle, &numQueueFamilies, nullptr);
    
    m_queueFamilies.resize(numQueueFamilies);
    m_vki->vkGetPhysicalDeviceQueueFamilyProperties(
      m_handle, &numQueueFamilies, m_queueFamilies.data());
  }
  
  
  DxvkAdapter::~DxvkAdapter() {
    
  }
  
  
  VkPhysicalDeviceProperties DxvkAdapter::deviceProperties() const {
    VkPhysicalDeviceProperties properties;
    m_vki->vkGetPhysicalDeviceProperties(m_handle, &properties);
    return properties;
  }
  
  
  VkPhysicalDeviceMemoryProperties DxvkAdapter::memoryProperties() const {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    m_vki->vkGetPhysicalDeviceMemoryProperties(m_handle, &memoryProperties);
    return memoryProperties;
  }
  
  
  VkPhysicalDeviceFeatures DxvkAdapter::features() const {
    VkPhysicalDeviceFeatures features;
    m_vki->vkGetPhysicalDeviceFeatures(m_handle, &features);
    return features;
  }
  
  
  VkFormatProperties DxvkAdapter::formatProperties(VkFormat format) const {
    VkFormatProperties formatProperties;
    m_vki->vkGetPhysicalDeviceFormatProperties(m_handle, format, &formatProperties);
    return formatProperties;
  }
  
    
  std::optional<VkImageFormatProperties> DxvkAdapter::imageFormatProperties(
    VkFormat            format,
    VkImageType         type,
    VkImageTiling       tiling,
    VkImageUsageFlags   usage,
    VkImageCreateFlags  flags) const {
    VkImageFormatProperties formatProperties;
    
    VkResult status = m_vki->vkGetPhysicalDeviceImageFormatProperties(
      m_handle, format, type, tiling, usage, flags, &formatProperties);
    
    switch (status) {
      case VK_SUCCESS:                    return formatProperties;
      case VK_ERROR_FORMAT_NOT_SUPPORTED: return { };
      
      default:
        throw DxvkError("DxvkAdapter::imageFormatProperties: Failed to query format properties");
    }
  }
  
    
  uint32_t DxvkAdapter::graphicsQueueFamily() const {
    for (uint32_t i = 0; i < m_queueFamilies.size(); i++) {
      if (m_queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        return i;
    }
    
    throw DxvkError("DxvkAdapter::graphicsQueueFamily: No graphics queue found");
  }
  
  
  uint32_t DxvkAdapter::presentQueueFamily() const {
    // TODO Implement properly
    return this->graphicsQueueFamily();
  }
  
  
  Rc<DxvkDevice> DxvkAdapter::createDevice() {
    auto enabledExtensions = this->enableExtensions();
    auto enabledFeatures   = this->enableFeatures();
    
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    
    const uint32_t gIndex = this->graphicsQueueFamily();
    const uint32_t pIndex = this->presentQueueFamily();
    
    VkDeviceQueueCreateInfo graphicsQueue;
    graphicsQueue.sType             = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueue.pNext             = nullptr;
    graphicsQueue.flags             = 0;
    graphicsQueue.queueFamilyIndex  = gIndex;
    graphicsQueue.queueCount        = 1;
    graphicsQueue.pQueuePriorities  = &queuePriority;
    queueInfos.push_back(graphicsQueue);
    
    if (pIndex != gIndex) {
      VkDeviceQueueCreateInfo presentQueue = graphicsQueue;
      presentQueue.queueFamilyIndex        = pIndex;
      queueInfos.push_back(presentQueue);
    }
    
    VkDeviceCreateInfo info;
    info.sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext                      = nullptr;
    info.flags                      = 0;
    info.queueCreateInfoCount       = queueInfos.size();
    info.pQueueCreateInfos          = queueInfos.data();
    info.enabledLayerCount          = 0;
    info.ppEnabledLayerNames        = nullptr;
    info.enabledExtensionCount      = enabledExtensions.count();
    info.ppEnabledExtensionNames    = enabledExtensions.names();
    info.pEnabledFeatures           = &enabledFeatures;
    
    VkDevice device = VK_NULL_HANDLE;
    
    if (m_vki->vkCreateDevice(m_handle, &info, nullptr, &device) != VK_SUCCESS)
      throw DxvkError("DxvkDevice::createDevice: Failed to create device");
    return new DxvkDevice(this, new vk::DeviceFn(m_vki->instance(), device));
  }
  
  
  Rc<DxvkSurface> DxvkAdapter::createSurface(HINSTANCE instance, HWND window) {
    return new DxvkSurface(this, instance, window);
  }
  
  
  vk::NameList DxvkAdapter::enableExtensions() {
    std::vector<const char*> extOptional = { };
    std::vector<const char*> extRequired = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    
    const vk::NameSet extensionsAvailable
      = vk::NameSet::enumerateDeviceExtensions(*m_vki, m_handle);
    vk::NameList extensionsEnabled;
    
    for (auto e : extOptional) {
      if (extensionsAvailable.supports(e))
        extensionsEnabled.add(e);
    }
    
    for (auto e : extRequired) {
      if (!extensionsAvailable.supports(e))
        throw DxvkError(str::format("DxvkDevice::getExtensions: Extension ", e, " not supported"));
      extensionsEnabled.add(e);
    }
    
    return extensionsEnabled;
  }
  
  
  VkPhysicalDeviceFeatures DxvkAdapter::enableFeatures() {
    VkPhysicalDeviceFeatures features;
    std::memset(&features, 0, sizeof(features));
    return features;
  }
  
}