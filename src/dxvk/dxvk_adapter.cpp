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
    
    if (DxvkGpuVendor(properties.vendorID) == DxvkGpuVendor::Nvidia) {
      properties.driverVersion = VK_MAKE_VERSION(
        VK_VERSION_MAJOR(properties.driverVersion),
        VK_VERSION_MINOR(properties.driverVersion) >> 2,
        VK_VERSION_PATCH(properties.driverVersion));
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
    const Rc<DxvkDeviceExtensions> extensions = new DxvkDeviceExtensions();
    extensions->enableExtensions(vk::NameSet::enumerateDeviceExtensions(*m_vki, m_handle));
    
    if (!extensions->checkSupportStatus())
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    const vk::NameList enabledExtensions =
      extensions->getEnabledExtensionNames();
    
    Logger::info("Enabled device extensions:");
    this->logNameList(enabledExtensions);
    
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
  
  
  void DxvkAdapter::logNameList(const vk::NameList& names) {
    for (uint32_t i = 0; i < names.count(); i++)
      Logger::info(str::format("  ", names.name(i)));
  }
  
}