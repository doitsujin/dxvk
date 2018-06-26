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
  
  
  Rc<DxvkInstance> DxvkAdapter::instance() const {
    return m_instance;
  }
  
  
  VkPhysicalDeviceProperties DxvkAdapter::deviceProperties() const {
    VkPhysicalDeviceProperties properties;
    m_vki->vkGetPhysicalDeviceProperties(m_handle, &properties);
    
    if (DxvkGpuVendor(properties.vendorID) == DxvkGpuVendor::Nvidia) {
      properties.driverVersion = VK_MAKE_VERSION(
        VK_VERSION_MAJOR(properties.driverVersion),
        VK_VERSION_MINOR(properties.driverVersion >> 0) >> 2,
        VK_VERSION_PATCH(properties.driverVersion >> 2) >> 4);
    }
    
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
  
    
  VkResult DxvkAdapter::imageFormatProperties(
    VkFormat                  format,
    VkImageType               type,
    VkImageTiling             tiling,
    VkImageUsageFlags         usage,
    VkImageCreateFlags        flags,
    VkImageFormatProperties&  properties) const {
    return m_vki->vkGetPhysicalDeviceImageFormatProperties(
      m_handle, format, type, tiling, usage, flags, &properties);
  }
  
    
  uint32_t DxvkAdapter::graphicsQueueFamily() const {
    for (uint32_t i = 0; i < m_queueFamilies.size(); i++) {
      if (m_queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        return i;
    }
    
    throw DxvkError("DxvkAdapter: No graphics queue found");
  }
  
  
  uint32_t DxvkAdapter::presentQueueFamily() const {
    // TODO Implement properly
    return this->graphicsQueueFamily();
  }
  
  
  bool DxvkAdapter::checkFeatureSupport(
    const VkPhysicalDeviceFeatures& required) const {
    const VkPhysicalDeviceFeatures supported = this->features();
    
    return (supported.robustBufferAccess || !required.robustBufferAccess)
        && (supported.fullDrawIndexUint32 || !required.fullDrawIndexUint32)
        && (supported.imageCubeArray || !required.imageCubeArray)
        && (supported.independentBlend || !required.independentBlend)
        && (supported.geometryShader || !required.geometryShader)
        && (supported.tessellationShader || !required.tessellationShader)
        && (supported.sampleRateShading || !required.sampleRateShading)
        && (supported.dualSrcBlend || !required.dualSrcBlend)
        && (supported.logicOp || !required.logicOp)
        && (supported.multiDrawIndirect || !required.multiDrawIndirect)
        && (supported.drawIndirectFirstInstance || !required.drawIndirectFirstInstance)
        && (supported.depthClamp || !required.depthClamp)
        && (supported.depthBiasClamp || !required.depthBiasClamp)
        && (supported.fillModeNonSolid || !required.fillModeNonSolid)
        && (supported.depthBounds || !required.depthBounds)
        && (supported.wideLines || !required.wideLines)
        && (supported.largePoints || !required.largePoints)
        && (supported.alphaToOne || !required.alphaToOne)
        && (supported.multiViewport || !required.multiViewport)
        && (supported.samplerAnisotropy || !required.samplerAnisotropy)
        && (supported.textureCompressionETC2 || !required.textureCompressionETC2)
        && (supported.textureCompressionASTC_LDR || !required.textureCompressionASTC_LDR)
        && (supported.textureCompressionBC || !required.textureCompressionBC)
        && (supported.occlusionQueryPrecise || !required.occlusionQueryPrecise)
        && (supported.pipelineStatisticsQuery || !required.pipelineStatisticsQuery)
        && (supported.vertexPipelineStoresAndAtomics || !required.vertexPipelineStoresAndAtomics)
        && (supported.fragmentStoresAndAtomics || !required.fragmentStoresAndAtomics)
        && (supported.shaderTessellationAndGeometryPointSize || !required.shaderTessellationAndGeometryPointSize)
        && (supported.shaderImageGatherExtended || !required.shaderImageGatherExtended)
        && (supported.shaderStorageImageExtendedFormats || !required.shaderStorageImageExtendedFormats)
        && (supported.shaderStorageImageMultisample || !required.shaderStorageImageMultisample)
        && (supported.shaderStorageImageReadWithoutFormat || !required.shaderStorageImageReadWithoutFormat)
        && (supported.shaderStorageImageWriteWithoutFormat || !required.shaderStorageImageWriteWithoutFormat)
        && (supported.shaderUniformBufferArrayDynamicIndexing || !required.shaderUniformBufferArrayDynamicIndexing)
        && (supported.shaderSampledImageArrayDynamicIndexing || !required.shaderSampledImageArrayDynamicIndexing)
        && (supported.shaderStorageBufferArrayDynamicIndexing || !required.shaderStorageBufferArrayDynamicIndexing)
        && (supported.shaderStorageImageArrayDynamicIndexing || !required.shaderStorageImageArrayDynamicIndexing)
        && (supported.shaderClipDistance || !required.shaderClipDistance)
        && (supported.shaderCullDistance || !required.shaderCullDistance)
        && (supported.shaderFloat64 || !required.shaderFloat64)
        && (supported.shaderInt64 || !required.shaderInt64)
        && (supported.shaderInt16 || !required.shaderInt16)
        && (supported.shaderResourceResidency || !required.shaderResourceResidency)
        && (supported.shaderResourceMinLod || !required.shaderResourceMinLod)
        && (supported.sparseBinding || !required.sparseBinding)
        && (supported.sparseResidencyBuffer || !required.sparseResidencyBuffer)
        && (supported.sparseResidencyImage2D || !required.sparseResidencyImage2D)
        && (supported.sparseResidencyImage3D || !required.sparseResidencyImage3D)
        && (supported.sparseResidency2Samples || !required.sparseResidency2Samples)
        && (supported.sparseResidency4Samples || !required.sparseResidency4Samples)
        && (supported.sparseResidency8Samples || !required.sparseResidency8Samples)
        && (supported.sparseResidency16Samples || !required.sparseResidency16Samples)
        && (supported.sparseResidencyAliased || !required.sparseResidencyAliased)
        && (supported.variableMultisampleRate || !required.variableMultisampleRate)
        && (supported.inheritedQueries || !required.inheritedQueries);
  }
  
  
  Rc<DxvkDevice> DxvkAdapter::createDevice(const VkPhysicalDeviceFeatures& enabledFeatures) {
    // Query available extensions and enable the ones that are needed
    vk::NameSet availableExtensions = vk::NameSet::enumerateDeviceExtensions(*m_vki, m_handle);
    
    const Rc<DxvkDeviceExtensions> extensions = new DxvkDeviceExtensions();
    extensions->enableExtensions(availableExtensions);
    
    if (!extensions->checkSupportStatus())
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    // Generate list of extensions that we're actually going to use
    vk::NameSet enabledExtensionSet = extensions->getEnabledExtensionNames();
    enabledExtensionSet.merge(g_vrInstance.getDeviceExtensions(getAdapterIndex()));
    
    vk::NameList enabledExtensionList = enabledExtensionSet.getNameList();
    
    Logger::info("Enabled device extensions:");
    this->logNameList(enabledExtensionList);
    
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    
    uint32_t gIndex = this->graphicsQueueFamily();
    uint32_t pIndex = this->presentQueueFamily();
    
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
    info.enabledExtensionCount      = enabledExtensionList.count();
    info.ppEnabledExtensionNames    = enabledExtensionList.names();
    info.pEnabledFeatures           = &enabledFeatures;
    
    VkDevice device = VK_NULL_HANDLE;
    
    if (m_vki->vkCreateDevice(m_handle, &info, nullptr, &device) != VK_SUCCESS)
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    Rc<DxvkDevice> result = new DxvkDevice(this,
      new vk::DeviceFn(m_vki->instance(), device),
      extensions, enabledFeatures);
    result->initResources();
    return result;
  }
  
  
  Rc<DxvkSurface> DxvkAdapter::createSurface(HINSTANCE instance, HWND window) {
    return new DxvkSurface(this, instance, window);
  }
  
  
  void DxvkAdapter::logAdapterInfo() const {
    VkPhysicalDeviceProperties deviceInfo = this->deviceProperties();
    VkPhysicalDeviceMemoryProperties memoryInfo = this->memoryProperties();
    
    Logger::info(str::format(deviceInfo.deviceName, ":"));
    Logger::info(str::format("  Driver: ",
      VK_VERSION_MAJOR(deviceInfo.driverVersion), ".",
      VK_VERSION_MINOR(deviceInfo.driverVersion), ".",
      VK_VERSION_PATCH(deviceInfo.driverVersion)));
    Logger::info(str::format("  Vulkan: ",
      VK_VERSION_MAJOR(deviceInfo.apiVersion), ".",
      VK_VERSION_MINOR(deviceInfo.apiVersion), ".",
      VK_VERSION_PATCH(deviceInfo.apiVersion)));

    for (uint32_t i = 0; i < memoryInfo.memoryHeapCount; i++) {
      constexpr VkDeviceSize mib = 1024 * 1024;
      
      Logger::info(str::format("  Memory Heap[", i, "]: "));
      Logger::info(str::format("    Size: ", memoryInfo.memoryHeaps[i].size / mib, " MiB"));
      Logger::info(str::format("    Flags: ", "0x", std::hex, memoryInfo.memoryHeaps[i].flags));
      
      for (uint32_t j = 0; j < memoryInfo.memoryTypeCount; j++) {
        if (memoryInfo.memoryTypes[j].heapIndex == i) {
          Logger::info(str::format(
            "    Memory Type[", j, "]: ",
            "Property Flags = ", "0x", std::hex, memoryInfo.memoryTypes[j].propertyFlags));
        }
      }
    }
  }
  
  
  uint32_t DxvkAdapter::getAdapterIndex() const {
    for (uint32_t i = 0; m_instance->enumAdapters(i) != nullptr; i++) {
      if (m_instance->enumAdapters(i).ptr() == this)
        return i;
    }

    return ~0u;
  }
  
  
  void DxvkAdapter::logNameList(const vk::NameList& names) {
    for (uint32_t i = 0; i < names.count(); i++)
      Logger::info(str::format("  ", names.name(i)));
  }
  
}
