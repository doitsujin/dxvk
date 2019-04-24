#include <cstring>

#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkAdapter::DxvkAdapter(
          DxvkInstance*       instance,
          VkPhysicalDevice    handle)
  : m_instance      (instance),
    m_vki           (instance->vki()),
    m_handle        (handle) {
    this->initHeapAllocInfo();
    this->queryExtensions();
    this->queryDeviceInfo();
    this->queryDeviceFeatures();
    this->queryDeviceQueues();

    m_hasMemoryBudget = m_deviceExtensions.supports(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
  }
  
  
  DxvkAdapter::~DxvkAdapter() {
    
  }
  
  
  Rc<DxvkInstance> DxvkAdapter::instance() const {
    return m_instance;
  }
  
  
  DxvkAdapterMemoryInfo DxvkAdapter::getMemoryHeapInfo() const {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudget = { };
    memBudget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    memBudget.pNext = nullptr;

    VkPhysicalDeviceMemoryProperties2KHR memProps = { };
    memProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR;
    memProps.pNext = m_hasMemoryBudget ? &memBudget : nullptr;

    m_vki->vkGetPhysicalDeviceMemoryProperties2KHR(m_handle, &memProps);
    
    DxvkAdapterMemoryInfo info = { };
    info.heapCount = memProps.memoryProperties.memoryHeapCount;

    for (uint32_t i = 0; i < info.heapCount; i++) {
      info.heaps[i].heapFlags = memProps.memoryProperties.memoryHeaps[i].flags;

      if (m_hasMemoryBudget) {
        info.heaps[i].memoryAvailable = memBudget.heapBudget[i];
        info.heaps[i].memoryAllocated = memBudget.heapUsage[i];
      } else {
        info.heaps[i].memoryAvailable = memProps.memoryProperties.memoryHeaps[i].size;
        info.heaps[i].memoryAllocated = m_heapAlloc[i].load();
      }
    }

    return info;
  }


  VkPhysicalDeviceMemoryProperties DxvkAdapter::memoryProperties() const {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    m_vki->vkGetPhysicalDeviceMemoryProperties(m_handle, &memoryProperties);
    return memoryProperties;
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
  
  
  bool DxvkAdapter::checkFeatureSupport(const DxvkDeviceFeatures& required) const {
    return (m_deviceFeatures.core.features.robustBufferAccess
                || !required.core.features.robustBufferAccess)
        && (m_deviceFeatures.core.features.fullDrawIndexUint32
                || !required.core.features.fullDrawIndexUint32)
        && (m_deviceFeatures.core.features.imageCubeArray
                || !required.core.features.imageCubeArray)
        && (m_deviceFeatures.core.features.independentBlend
                || !required.core.features.independentBlend)
        && (m_deviceFeatures.core.features.geometryShader
                || !required.core.features.geometryShader)
        && (m_deviceFeatures.core.features.tessellationShader
                || !required.core.features.tessellationShader)
        && (m_deviceFeatures.core.features.sampleRateShading
                || !required.core.features.sampleRateShading)
        && (m_deviceFeatures.core.features.dualSrcBlend
                || !required.core.features.dualSrcBlend)
        && (m_deviceFeatures.core.features.logicOp
                || !required.core.features.logicOp)
        && (m_deviceFeatures.core.features.multiDrawIndirect
                || !required.core.features.multiDrawIndirect)
        && (m_deviceFeatures.core.features.drawIndirectFirstInstance
                || !required.core.features.drawIndirectFirstInstance)
        && (m_deviceFeatures.core.features.depthClamp
                || !required.core.features.depthClamp)
        && (m_deviceFeatures.core.features.depthBiasClamp
                || !required.core.features.depthBiasClamp)
        && (m_deviceFeatures.core.features.fillModeNonSolid
                || !required.core.features.fillModeNonSolid)
        && (m_deviceFeatures.core.features.depthBounds
                || !required.core.features.depthBounds)
        && (m_deviceFeatures.core.features.wideLines
                || !required.core.features.wideLines)
        && (m_deviceFeatures.core.features.largePoints
                || !required.core.features.largePoints)
        && (m_deviceFeatures.core.features.alphaToOne
                || !required.core.features.alphaToOne)
        && (m_deviceFeatures.core.features.multiViewport
                || !required.core.features.multiViewport)
        && (m_deviceFeatures.core.features.samplerAnisotropy
                || !required.core.features.samplerAnisotropy)
        && (m_deviceFeatures.core.features.textureCompressionETC2
                || !required.core.features.textureCompressionETC2)
        && (m_deviceFeatures.core.features.textureCompressionASTC_LDR
                || !required.core.features.textureCompressionASTC_LDR)
        && (m_deviceFeatures.core.features.textureCompressionBC
                || !required.core.features.textureCompressionBC)
        && (m_deviceFeatures.core.features.occlusionQueryPrecise
                || !required.core.features.occlusionQueryPrecise)
        && (m_deviceFeatures.core.features.pipelineStatisticsQuery
                || !required.core.features.pipelineStatisticsQuery)
        && (m_deviceFeatures.core.features.vertexPipelineStoresAndAtomics
                || !required.core.features.vertexPipelineStoresAndAtomics)
        && (m_deviceFeatures.core.features.fragmentStoresAndAtomics
                || !required.core.features.fragmentStoresAndAtomics)
        && (m_deviceFeatures.core.features.shaderTessellationAndGeometryPointSize
                || !required.core.features.shaderTessellationAndGeometryPointSize)
        && (m_deviceFeatures.core.features.shaderImageGatherExtended
                || !required.core.features.shaderImageGatherExtended)
        && (m_deviceFeatures.core.features.shaderStorageImageExtendedFormats
                || !required.core.features.shaderStorageImageExtendedFormats)
        && (m_deviceFeatures.core.features.shaderStorageImageMultisample
                || !required.core.features.shaderStorageImageMultisample)
        && (m_deviceFeatures.core.features.shaderStorageImageReadWithoutFormat
                || !required.core.features.shaderStorageImageReadWithoutFormat)
        && (m_deviceFeatures.core.features.shaderStorageImageWriteWithoutFormat
                || !required.core.features.shaderStorageImageWriteWithoutFormat)
        && (m_deviceFeatures.core.features.shaderUniformBufferArrayDynamicIndexing
                || !required.core.features.shaderUniformBufferArrayDynamicIndexing)
        && (m_deviceFeatures.core.features.shaderSampledImageArrayDynamicIndexing
                || !required.core.features.shaderSampledImageArrayDynamicIndexing)
        && (m_deviceFeatures.core.features.shaderStorageBufferArrayDynamicIndexing
                || !required.core.features.shaderStorageBufferArrayDynamicIndexing)
        && (m_deviceFeatures.core.features.shaderStorageImageArrayDynamicIndexing
                || !required.core.features.shaderStorageImageArrayDynamicIndexing)
        && (m_deviceFeatures.core.features.shaderClipDistance
                || !required.core.features.shaderClipDistance)
        && (m_deviceFeatures.core.features.shaderCullDistance
                || !required.core.features.shaderCullDistance)
        && (m_deviceFeatures.core.features.shaderFloat64
                || !required.core.features.shaderFloat64)
        && (m_deviceFeatures.core.features.shaderInt64
                || !required.core.features.shaderInt64)
        && (m_deviceFeatures.core.features.shaderInt16
                || !required.core.features.shaderInt16)
        && (m_deviceFeatures.core.features.shaderResourceResidency
                || !required.core.features.shaderResourceResidency)
        && (m_deviceFeatures.core.features.shaderResourceMinLod
                || !required.core.features.shaderResourceMinLod)
        && (m_deviceFeatures.core.features.sparseBinding
                || !required.core.features.sparseBinding)
        && (m_deviceFeatures.core.features.sparseResidencyBuffer
                || !required.core.features.sparseResidencyBuffer)
        && (m_deviceFeatures.core.features.sparseResidencyImage2D
                || !required.core.features.sparseResidencyImage2D)
        && (m_deviceFeatures.core.features.sparseResidencyImage3D
                || !required.core.features.sparseResidencyImage3D)
        && (m_deviceFeatures.core.features.sparseResidency2Samples
                || !required.core.features.sparseResidency2Samples)
        && (m_deviceFeatures.core.features.sparseResidency4Samples
                || !required.core.features.sparseResidency4Samples)
        && (m_deviceFeatures.core.features.sparseResidency8Samples
                || !required.core.features.sparseResidency8Samples)
        && (m_deviceFeatures.core.features.sparseResidency16Samples
                || !required.core.features.sparseResidency16Samples)
        && (m_deviceFeatures.core.features.sparseResidencyAliased
                || !required.core.features.sparseResidencyAliased)
        && (m_deviceFeatures.core.features.variableMultisampleRate
                || !required.core.features.variableMultisampleRate)
        && (m_deviceFeatures.core.features.inheritedQueries
                || !required.core.features.inheritedQueries)
        && (m_deviceFeatures.extConditionalRendering.conditionalRendering
                || !required.extConditionalRendering.conditionalRendering)
        && (m_deviceFeatures.extDepthClipEnable.depthClipEnable
                || !required.extDepthClipEnable.depthClipEnable)
        && (m_deviceFeatures.extHostQueryReset.hostQueryReset
                || !required.extHostQueryReset.hostQueryReset)
        && (m_deviceFeatures.extMemoryPriority.memoryPriority
                || !required.extMemoryPriority.memoryPriority)
        && (m_deviceFeatures.extTransformFeedback.transformFeedback
                || !required.extTransformFeedback.transformFeedback)
        && (m_deviceFeatures.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor
                || !required.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor)
        && (m_deviceFeatures.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor
                || !required.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor);
  }
  
  
  void DxvkAdapter::enableExtensions(const DxvkNameSet& extensions) {
    m_extraExtensions.merge(extensions);
  }


  Rc<DxvkDevice> DxvkAdapter::createDevice(std::string clientApi, DxvkDeviceFeatures enabledFeatures) {
    DxvkDeviceExtensions devExtensions;

    std::array<DxvkExt*, 20> devExtensionList = {{
      &devExtensions.amdMemoryOverallocationBehaviour,
      &devExtensions.amdShaderFragmentMask,
      &devExtensions.extConditionalRendering,
      &devExtensions.extDepthClipEnable,
      &devExtensions.extHostQueryReset,
      &devExtensions.extMemoryPriority,
      &devExtensions.extShaderViewportIndexLayer,
      &devExtensions.extTransformFeedback,
      &devExtensions.extVertexAttributeDivisor,
      &devExtensions.khrDedicatedAllocation,
      &devExtensions.khrDescriptorUpdateTemplate,
      &devExtensions.khrDrawIndirectCount,
      &devExtensions.khrDriverProperties,
      &devExtensions.khrGetMemoryRequirements2,
      &devExtensions.khrImageFormatList,
      &devExtensions.khrMaintenance1,
      &devExtensions.khrMaintenance2,
      &devExtensions.khrSamplerMirrorClampToEdge,
      &devExtensions.khrShaderDrawParameters,
      &devExtensions.khrSwapchain,
    }};

    DxvkNameSet extensionsEnabled;

    if (!m_deviceExtensions.enableExtensions(
          devExtensionList.size(),
          devExtensionList.data(),
          extensionsEnabled))
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    // Enable additional extensions if necessary
    extensionsEnabled.merge(m_extraExtensions);
    DxvkNameList extensionNameList = extensionsEnabled.toNameList();
    
    Logger::info("Enabled device extensions:");
    this->logNameList(extensionNameList);

    // Create pNext chain for additional device features
    enabledFeatures.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    enabledFeatures.core.pNext = nullptr;

    if (devExtensions.extConditionalRendering) {
      enabledFeatures.extConditionalRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
      enabledFeatures.extConditionalRendering.pNext = enabledFeatures.core.pNext;
      enabledFeatures.core.pNext = &enabledFeatures.extConditionalRendering;
    }

    if (devExtensions.extDepthClipEnable) {
      enabledFeatures.extDepthClipEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
      enabledFeatures.extDepthClipEnable.pNext = enabledFeatures.core.pNext;
      enabledFeatures.core.pNext = &enabledFeatures.extDepthClipEnable;
    }

    if (devExtensions.extHostQueryReset) {
      enabledFeatures.extHostQueryReset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
      enabledFeatures.extHostQueryReset.pNext = enabledFeatures.core.pNext;
      enabledFeatures.core.pNext = &enabledFeatures.extHostQueryReset;
    }

    if (devExtensions.extMemoryPriority) {
      enabledFeatures.extMemoryPriority.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
      enabledFeatures.extMemoryPriority.pNext = enabledFeatures.core.pNext;
      enabledFeatures.core.pNext = &enabledFeatures.extMemoryPriority;
    }

    if (devExtensions.extTransformFeedback) {
      enabledFeatures.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
      enabledFeatures.extTransformFeedback.pNext = enabledFeatures.core.pNext;
      enabledFeatures.core.pNext = &enabledFeatures.extTransformFeedback;
    }

    if (devExtensions.extVertexAttributeDivisor.revision() >= 3) {
      enabledFeatures.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
      enabledFeatures.extVertexAttributeDivisor.pNext = enabledFeatures.core.pNext;
      enabledFeatures.core.pNext = &enabledFeatures.extVertexAttributeDivisor;
    }

    // Report the desired overallocation behaviour to the driver
    VkDeviceMemoryOverallocationCreateInfoAMD overallocInfo;
    overallocInfo.sType = VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD;
    overallocInfo.pNext = nullptr;
    overallocInfo.overallocationBehavior = VK_MEMORY_OVERALLOCATION_BEHAVIOR_ALLOWED_AMD;
    
    // Create one single queue for graphics and present
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
    info.pNext                      = enabledFeatures.core.pNext;
    info.flags                      = 0;
    info.queueCreateInfoCount       = queueInfos.size();
    info.pQueueCreateInfos          = queueInfos.data();
    info.enabledLayerCount          = 0;
    info.ppEnabledLayerNames        = nullptr;
    info.enabledExtensionCount      = extensionNameList.count();
    info.ppEnabledExtensionNames    = extensionNameList.names();
    info.pEnabledFeatures           = &enabledFeatures.core.features;

    if (devExtensions.amdMemoryOverallocationBehaviour)
      overallocInfo.pNext = std::exchange(info.pNext, &overallocInfo);
    
    VkDevice device = VK_NULL_HANDLE;
    
    if (m_vki->vkCreateDevice(m_handle, &info, nullptr, &device) != VK_SUCCESS)
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    Rc<DxvkDevice> result = new DxvkDevice(clientApi, this,
      new vk::DeviceFn(true, m_vki->instance(), device),
      devExtensions, enabledFeatures);
    result->initResources();
    return result;
  }
  
  
  void DxvkAdapter::notifyHeapMemoryAlloc(
          uint32_t            heap,
          VkDeviceSize        bytes) {
    if (!m_hasMemoryBudget)
      m_heapAlloc[heap] += bytes;
  }

  
  void DxvkAdapter::notifyHeapMemoryFree(
          uint32_t            heap,
          VkDeviceSize        bytes) {
    if (!m_hasMemoryBudget)
      m_heapAlloc[heap] -= bytes;
  }


  bool DxvkAdapter::matchesDriver(
          DxvkGpuVendor       vendor,
          VkDriverIdKHR       driver,
          uint32_t            minVer,
          uint32_t            maxVer) const {
    bool driverMatches = m_deviceInfo.khrDeviceDriverProperties.driverID
      ? driver == m_deviceInfo.khrDeviceDriverProperties.driverID
      : vendor == DxvkGpuVendor(m_deviceInfo.core.properties.vendorID);

    if (minVer) driverMatches &= m_deviceInfo.core.properties.driverVersion >= minVer;
    if (maxVer) driverMatches &= m_deviceInfo.core.properties.driverVersion <  maxVer;

    return driverMatches;
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
  
  
  void DxvkAdapter::initHeapAllocInfo() {
    for (uint32_t i = 0; i < m_heapAlloc.size(); i++)
      m_heapAlloc[i] = 0;
  }


  void DxvkAdapter::queryExtensions() {
    m_deviceExtensions = DxvkNameSet::enumDeviceExtensions(m_vki, m_handle);
  }


  void DxvkAdapter::queryDeviceInfo() {
    m_deviceInfo = DxvkDeviceInfo();
    m_deviceInfo.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    m_deviceInfo.core.pNext = nullptr;

    // Query info now so that we have basic device properties available
    m_vki->vkGetPhysicalDeviceProperties2KHR(m_handle, &m_deviceInfo.core);

    if (m_deviceInfo.core.properties.apiVersion >= VK_MAKE_VERSION(1, 1, 0)) {
      m_deviceInfo.coreDeviceId.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
      m_deviceInfo.coreDeviceId.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.coreDeviceId);

      m_deviceInfo.coreSubgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
      m_deviceInfo.coreSubgroup.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.coreSubgroup);
    }

    if (m_deviceExtensions.supports(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)) {
      m_deviceInfo.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
      m_deviceInfo.extTransformFeedback.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extTransformFeedback);
    }

    if (m_deviceExtensions.supports(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME)) {
      m_deviceInfo.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
      m_deviceInfo.extVertexAttributeDivisor.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extVertexAttributeDivisor);
    }

    if (m_deviceExtensions.supports(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
      m_deviceInfo.khrDeviceDriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
      m_deviceInfo.khrDeviceDriverProperties.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.khrDeviceDriverProperties);
    }

    // Query full device properties for all enabled extensions
    m_vki->vkGetPhysicalDeviceProperties2KHR(m_handle, &m_deviceInfo.core);
    
    // Nvidia reports the driver version in a slightly different format
    if (DxvkGpuVendor(m_deviceInfo.core.properties.vendorID) == DxvkGpuVendor::Nvidia) {
      m_deviceInfo.core.properties.driverVersion = VK_MAKE_VERSION(
        VK_VERSION_MAJOR(m_deviceInfo.core.properties.driverVersion),
        VK_VERSION_MINOR(m_deviceInfo.core.properties.driverVersion >> 0) >> 2,
        VK_VERSION_PATCH(m_deviceInfo.core.properties.driverVersion >> 2) >> 4);
    }
  }


  void DxvkAdapter::queryDeviceFeatures() {
    m_deviceFeatures = DxvkDeviceFeatures();
    m_deviceFeatures.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    m_deviceFeatures.core.pNext = nullptr;

    if (m_deviceExtensions.supports(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME)) {
      m_deviceFeatures.extConditionalRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT;
      m_deviceFeatures.extConditionalRendering.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extConditionalRendering);
    }

    if (m_deviceExtensions.supports(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME)) {
      m_deviceFeatures.extDepthClipEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
      m_deviceFeatures.extDepthClipEnable.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extDepthClipEnable);
    }

    if (m_deviceExtensions.supports(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME)) {
      m_deviceFeatures.extHostQueryReset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
      m_deviceFeatures.extHostQueryReset.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extHostQueryReset);
    }

    if (m_deviceExtensions.supports(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
      m_deviceFeatures.extMemoryPriority.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
      m_deviceFeatures.extMemoryPriority.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extMemoryPriority);
    }

    if (m_deviceExtensions.supports(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)) {
      m_deviceFeatures.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
      m_deviceFeatures.extTransformFeedback.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extTransformFeedback);
    }

    if (m_deviceExtensions.supports(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME) >= 3) {
      m_deviceFeatures.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
      m_deviceFeatures.extVertexAttributeDivisor.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extVertexAttributeDivisor);
    }

    m_vki->vkGetPhysicalDeviceFeatures2KHR(m_handle, &m_deviceFeatures.core);
  }


  void DxvkAdapter::queryDeviceQueues() {
    uint32_t numQueueFamilies = 0;
    m_vki->vkGetPhysicalDeviceQueueFamilyProperties(
      m_handle, &numQueueFamilies, nullptr);
    
    m_queueFamilies.resize(numQueueFamilies);
    m_vki->vkGetPhysicalDeviceQueueFamilyProperties(
      m_handle, &numQueueFamilies, m_queueFamilies.data());
  }
  
  
  void DxvkAdapter::logNameList(const DxvkNameList& names) {
    for (uint32_t i = 0; i < names.count(); i++)
      Logger::info(str::format("  ", names.name(i)));
  }
  
}
