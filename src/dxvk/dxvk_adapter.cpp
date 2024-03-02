#include <cstring>
#include <unordered_set>

#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {

  DxvkDeviceQueue getDeviceQueue(const Rc<vk::DeviceFn>& vkd, uint32_t family, uint32_t index) {
    DxvkDeviceQueue result = { };
    result.queueFamily = family;
    result.queueIndex = index;

    if (family != VK_QUEUE_FAMILY_IGNORED)
      vkd->vkGetDeviceQueue(vkd->device(), family, index, &result.queueHandle);

    return result;
  }


  DxvkAdapter::DxvkAdapter(
    const Rc<vk::InstanceFn>& vki,
          VkPhysicalDevice    handle)
  : m_vki           (vki),
    m_handle        (handle) {
    this->queryExtensions();
    this->queryDeviceInfo();
    this->queryDeviceFeatures();
    this->queryDeviceQueues();

    m_hasMemoryBudget = m_deviceExtensions.supports(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
  }
  
  
  DxvkAdapter::~DxvkAdapter() {
    
  }
  
  
  DxvkAdapterMemoryInfo DxvkAdapter::getMemoryHeapInfo() const {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    VkPhysicalDeviceMemoryProperties2 memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    memProps.pNext = m_hasMemoryBudget ? &memBudget : nullptr;

    m_vki->vkGetPhysicalDeviceMemoryProperties2(m_handle, &memProps);
    
    DxvkAdapterMemoryInfo info = { };
    info.heapCount = memProps.memoryProperties.memoryHeapCount;

    for (uint32_t i = 0; i < info.heapCount; i++) {
      info.heaps[i].heapFlags = memProps.memoryProperties.memoryHeaps[i].flags;
      info.heaps[i].heapSize = memProps.memoryProperties.memoryHeaps[i].size;

      if (m_hasMemoryBudget) {
        // Handle DXVK's memory allocations separately so that
        // freeing  resources actually is visible to applications.
        VkDeviceSize allocated = m_memoryAllocated[i].load();
        VkDeviceSize used = m_memoryUsed[i].load();

        info.heaps[i].memoryBudget    = memBudget.heapBudget[i];
        info.heaps[i].memoryAllocated = std::max(memBudget.heapUsage[i], allocated) - allocated + used;
      } else {
        info.heaps[i].memoryBudget    = memProps.memoryProperties.memoryHeaps[i].size;
        info.heaps[i].memoryAllocated = m_memoryUsed[i].load();
      }
    }

    return info;
  }


  VkPhysicalDeviceMemoryProperties DxvkAdapter::memoryProperties() const {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    m_vki->vkGetPhysicalDeviceMemoryProperties(m_handle, &memoryProperties);
    return memoryProperties;
  }
  
  
  DxvkFormatFeatures DxvkAdapter::getFormatFeatures(VkFormat format) const {
    VkFormatProperties3 properties3 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3 };
    VkFormatProperties2 properties2 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &properties3 };
    m_vki->vkGetPhysicalDeviceFormatProperties2(m_handle, format, &properties2);

    DxvkFormatFeatures result;
    result.optimal = properties3.optimalTilingFeatures;
    result.linear  = properties3.linearTilingFeatures;
    result.buffer  = properties3.bufferFeatures;
    return result;
  }


  std::optional<DxvkFormatLimits> DxvkAdapter::getFormatLimits(
    const DxvkFormatQuery&          query) const {
    VkPhysicalDeviceExternalImageFormatInfo externalInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO };
    externalInfo.handleType = query.handleType;

    VkPhysicalDeviceImageFormatInfo2 info = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
    info.format = query.format;
    info.type   = query.type;
    info.tiling = query.tiling;
    info.usage  = query.usage;
    info.flags  = query.flags;

    if (externalInfo.handleType)
      externalInfo.pNext = std::exchange(info.pNext, &externalInfo);

    VkExternalImageFormatProperties externalProperties = { VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES };
    VkImageFormatProperties2 properties = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };

    if (externalInfo.handleType)
      externalProperties.pNext = std::exchange(properties.pNext, &externalProperties);

    VkResult vr = m_vki->vkGetPhysicalDeviceImageFormatProperties2(
      m_handle, &info, &properties);

    if (vr != VK_SUCCESS)
      return std::nullopt;

    DxvkFormatLimits result = { };
    result.maxExtent        = properties.imageFormatProperties.maxExtent;
    result.maxMipLevels     = properties.imageFormatProperties.maxMipLevels;
    result.maxArrayLayers   = properties.imageFormatProperties.maxArrayLayers;
    result.sampleCounts     = properties.imageFormatProperties.sampleCounts;
    result.maxResourceSize  = properties.imageFormatProperties.maxResourceSize;
    result.externalFeatures = externalProperties.externalMemoryProperties.externalMemoryFeatures;
    return result;
  }


  DxvkAdapterQueueIndices DxvkAdapter::findQueueFamilies() const {
    uint32_t graphicsQueue = findQueueFamily(
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    
    uint32_t computeQueue = findQueueFamily(
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
      VK_QUEUE_COMPUTE_BIT);
    
    if (computeQueue == VK_QUEUE_FAMILY_IGNORED)
      computeQueue = graphicsQueue;

    uint32_t transferQueue = findQueueFamily(
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
      VK_QUEUE_TRANSFER_BIT);
    
    if (transferQueue == VK_QUEUE_FAMILY_IGNORED)
      transferQueue = computeQueue;

    uint32_t sparseQueue = VK_QUEUE_FAMILY_IGNORED;

    if (m_queueFamilies[graphicsQueue].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
      // Prefer using the graphics queue as a sparse binding queue
      sparseQueue = graphicsQueue;
    } else {
      sparseQueue = findQueueFamily(
        VK_QUEUE_SPARSE_BINDING_BIT,
        VK_QUEUE_SPARSE_BINDING_BIT);
    }

    DxvkAdapterQueueIndices queues;
    queues.graphics = graphicsQueue;
    queues.transfer = transferQueue;
    queues.sparse = sparseQueue;
    return queues;
  }

#define CHECK_FEATURE_NEED(feature) \
  (m_deviceFeatures.feature         \
       || !required.feature)

  bool DxvkAdapter::checkFeatureSupport(const DxvkDeviceFeatures& required) const {
    return CHECK_FEATURE_NEED(core.features.robustBufferAccess)
        && CHECK_FEATURE_NEED(core.features.fullDrawIndexUint32)
        && CHECK_FEATURE_NEED(core.features.imageCubeArray)
        && CHECK_FEATURE_NEED(core.features.independentBlend)
        && CHECK_FEATURE_NEED(core.features.geometryShader)
        && CHECK_FEATURE_NEED(core.features.tessellationShader)
        && CHECK_FEATURE_NEED(core.features.sampleRateShading)
        && CHECK_FEATURE_NEED(core.features.dualSrcBlend)
        && CHECK_FEATURE_NEED(core.features.logicOp)
        && CHECK_FEATURE_NEED(core.features.multiDrawIndirect)
        && CHECK_FEATURE_NEED(core.features.drawIndirectFirstInstance)
        && CHECK_FEATURE_NEED(core.features.depthClamp)
        && CHECK_FEATURE_NEED(core.features.depthBiasClamp)
        && CHECK_FEATURE_NEED(core.features.fillModeNonSolid)
        && CHECK_FEATURE_NEED(core.features.depthBounds)
        && CHECK_FEATURE_NEED(core.features.wideLines)
        && CHECK_FEATURE_NEED(core.features.largePoints)
        && CHECK_FEATURE_NEED(core.features.alphaToOne)
        && CHECK_FEATURE_NEED(core.features.multiViewport)
        && CHECK_FEATURE_NEED(core.features.samplerAnisotropy)
        && CHECK_FEATURE_NEED(core.features.textureCompressionETC2)
        && CHECK_FEATURE_NEED(core.features.textureCompressionASTC_LDR)
        && CHECK_FEATURE_NEED(core.features.textureCompressionBC)
        && CHECK_FEATURE_NEED(core.features.occlusionQueryPrecise)
        && CHECK_FEATURE_NEED(core.features.pipelineStatisticsQuery)
        && CHECK_FEATURE_NEED(core.features.vertexPipelineStoresAndAtomics)
        && CHECK_FEATURE_NEED(core.features.fragmentStoresAndAtomics)
        && CHECK_FEATURE_NEED(core.features.shaderTessellationAndGeometryPointSize)
        && CHECK_FEATURE_NEED(core.features.shaderImageGatherExtended)
        && CHECK_FEATURE_NEED(core.features.shaderStorageImageExtendedFormats)
        && CHECK_FEATURE_NEED(core.features.shaderStorageImageMultisample)
        && CHECK_FEATURE_NEED(core.features.shaderStorageImageReadWithoutFormat)
        && CHECK_FEATURE_NEED(core.features.shaderStorageImageWriteWithoutFormat)
        && CHECK_FEATURE_NEED(core.features.shaderUniformBufferArrayDynamicIndexing)
        && CHECK_FEATURE_NEED(core.features.shaderSampledImageArrayDynamicIndexing)
        && CHECK_FEATURE_NEED(core.features.shaderStorageBufferArrayDynamicIndexing)
        && CHECK_FEATURE_NEED(core.features.shaderStorageImageArrayDynamicIndexing)
        && CHECK_FEATURE_NEED(core.features.shaderClipDistance)
        && CHECK_FEATURE_NEED(core.features.shaderCullDistance)
        && CHECK_FEATURE_NEED(core.features.shaderFloat64)
        && CHECK_FEATURE_NEED(core.features.shaderInt64)
        && CHECK_FEATURE_NEED(core.features.shaderInt16)
        && CHECK_FEATURE_NEED(core.features.shaderResourceResidency)
        && CHECK_FEATURE_NEED(core.features.shaderResourceMinLod)
        && CHECK_FEATURE_NEED(core.features.sparseBinding)
        && CHECK_FEATURE_NEED(core.features.sparseResidencyBuffer)
        && CHECK_FEATURE_NEED(core.features.sparseResidencyImage2D)
        && CHECK_FEATURE_NEED(core.features.sparseResidencyImage3D)
        && CHECK_FEATURE_NEED(core.features.sparseResidency2Samples)
        && CHECK_FEATURE_NEED(core.features.sparseResidency4Samples)
        && CHECK_FEATURE_NEED(core.features.sparseResidency8Samples)
        && CHECK_FEATURE_NEED(core.features.sparseResidency16Samples)
        && CHECK_FEATURE_NEED(core.features.sparseResidencyAliased)
        && CHECK_FEATURE_NEED(core.features.variableMultisampleRate)
        && CHECK_FEATURE_NEED(core.features.inheritedQueries)
        && CHECK_FEATURE_NEED(vk11.shaderDrawParameters)
        && CHECK_FEATURE_NEED(vk12.samplerMirrorClampToEdge)
        && CHECK_FEATURE_NEED(vk12.drawIndirectCount)
        && CHECK_FEATURE_NEED(vk12.hostQueryReset)
        && CHECK_FEATURE_NEED(vk12.timelineSemaphore)
        && CHECK_FEATURE_NEED(vk12.bufferDeviceAddress)
        && CHECK_FEATURE_NEED(vk12.shaderOutputViewportIndex)
        && CHECK_FEATURE_NEED(vk12.shaderOutputLayer)
        && CHECK_FEATURE_NEED(vk13.pipelineCreationCacheControl)
        && CHECK_FEATURE_NEED(vk13.shaderDemoteToHelperInvocation)
        && CHECK_FEATURE_NEED(vk13.shaderZeroInitializeWorkgroupMemory)
        && CHECK_FEATURE_NEED(vk13.synchronization2)
        && CHECK_FEATURE_NEED(vk13.dynamicRendering)
        && CHECK_FEATURE_NEED(vk13.maintenance4)
        && CHECK_FEATURE_NEED(extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout)
        && CHECK_FEATURE_NEED(extConservativeRasterization)
        && CHECK_FEATURE_NEED(extCustomBorderColor.customBorderColors)
        && CHECK_FEATURE_NEED(extCustomBorderColor.customBorderColorWithoutFormat)
        && CHECK_FEATURE_NEED(extDepthClipEnable.depthClipEnable)
        && CHECK_FEATURE_NEED(extDepthBiasControl.depthBiasControl)
        && CHECK_FEATURE_NEED(extDepthBiasControl.leastRepresentableValueForceUnormRepresentation)
        && CHECK_FEATURE_NEED(extDepthBiasControl.floatRepresentation)
        && CHECK_FEATURE_NEED(extDepthBiasControl.depthBiasExact)
        && CHECK_FEATURE_NEED(extGraphicsPipelineLibrary.graphicsPipelineLibrary)
        && CHECK_FEATURE_NEED(extMemoryBudget)
        && CHECK_FEATURE_NEED(extMemoryPriority.memoryPriority)
        && CHECK_FEATURE_NEED(extNonSeamlessCubeMap.nonSeamlessCubeMap)
        && CHECK_FEATURE_NEED(extRobustness2.robustBufferAccess2)
        && CHECK_FEATURE_NEED(extRobustness2.robustImageAccess2)
        && CHECK_FEATURE_NEED(extRobustness2.nullDescriptor)
        && CHECK_FEATURE_NEED(extShaderModuleIdentifier.shaderModuleIdentifier)
        && CHECK_FEATURE_NEED(extShaderStencilExport)
        && CHECK_FEATURE_NEED(extSwapchainColorSpace)
        && CHECK_FEATURE_NEED(extSwapchainMaintenance1.swapchainMaintenance1)
        && CHECK_FEATURE_NEED(extHdrMetadata)
        && CHECK_FEATURE_NEED(extTransformFeedback.transformFeedback)
        && CHECK_FEATURE_NEED(extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor)
        && CHECK_FEATURE_NEED(extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor);
  }

#undef CHECK_FEATURE_NEED

  void DxvkAdapter::enableExtensions(const DxvkNameSet& extensions) {
    m_extraExtensions.merge(extensions);
  }


  Rc<DxvkDevice> DxvkAdapter::createDevice(
    const Rc<DxvkInstance>&   instance,
          DxvkDeviceFeatures  enabledFeatures) {
    DxvkDeviceExtensions devExtensions;
    auto devExtensionList = getExtensionList(devExtensions);

    // Only enable Cuda interop extensions in 64-bit builds in
    // order to avoid potential driver or address space issues.
    // VK_KHR_buffer_device_address is expensive on some drivers.
    bool enableCudaInterop = !env::is32BitHostPlatform() &&
      m_deviceExtensions.supports(devExtensions.nvxBinaryImport.name()) &&
      m_deviceExtensions.supports(devExtensions.nvxImageViewHandle.name()) &&
      m_deviceFeatures.vk12.bufferDeviceAddress;

    if (enableCudaInterop) {
      devExtensions.nvxBinaryImport.setMode(DxvkExtMode::Optional);
      devExtensions.nvxImageViewHandle.setMode(DxvkExtMode::Optional);

      enabledFeatures.vk12.bufferDeviceAddress = VK_TRUE;
    }

    DxvkNameSet extensionsEnabled;

    if (!m_deviceExtensions.enableExtensions(
          devExtensionList.size(),
          devExtensionList.data(),
          &extensionsEnabled))
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    // Enable additional extensions if necessary
    extensionsEnabled.merge(m_extraExtensions);
    DxvkNameList extensionNameList = extensionsEnabled.toNameList();

    // Always enable robust buffer access
    enabledFeatures.core.features.robustBufferAccess = VK_TRUE;

    // Enable variable multisample rate if supported
    enabledFeatures.core.features.variableMultisampleRate =
      m_deviceFeatures.core.features.variableMultisampleRate;

    // Always enable memory model so client APIs can use it
    enabledFeatures.vk12.vulkanMemoryModel = VK_TRUE;

    // Optionally used by some client API extensions
    enabledFeatures.vk12.drawIndirectCount =
      m_deviceFeatures.vk12.drawIndirectCount;

    // Required since we no longer have a fallback for GPU queries
    enabledFeatures.vk12.hostQueryReset = VK_TRUE;

    // Used by some internal shaders, and can be used by applications
    enabledFeatures.vk12.shaderOutputViewportIndex =
      m_deviceFeatures.vk12.shaderOutputViewportIndex;
    enabledFeatures.vk12.shaderOutputLayer =
      m_deviceFeatures.vk12.shaderOutputLayer;

    // Required for proper GPU synchronization
    enabledFeatures.vk12.timelineSemaphore = VK_TRUE;

    // Only enable the base image robustness feature if robustness 2 isn't
    // supported, since this is only a subset of what we actually want.
    enabledFeatures.vk13.robustImageAccess =
      m_deviceFeatures.vk13.robustImageAccess &&
      !m_deviceFeatures.extRobustness2.robustImageAccess2;

    // Only used in combination with pipeline libraries
    // right now, but enabling it won't hurt anyway
    enabledFeatures.vk13.pipelineCreationCacheControl =
      m_deviceFeatures.vk13.pipelineCreationCacheControl;

    // Core features that we're relying on in various places
    enabledFeatures.vk13.synchronization2 = VK_TRUE;
    enabledFeatures.vk13.dynamicRendering = VK_TRUE;

    // We expose depth clip rather than depth clamp to client APIs
    enabledFeatures.extDepthClipEnable.depthClipEnable =
      m_deviceFeatures.extDepthClipEnable.depthClipEnable;

    // Used to make pipeline library stuff less clunky
    enabledFeatures.extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable =
      m_deviceFeatures.extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable;
    enabledFeatures.extExtendedDynamicState3.extendedDynamicState3DepthClipEnable =
      m_deviceFeatures.extExtendedDynamicState3.extendedDynamicState3DepthClipEnable &&
      m_deviceFeatures.extDepthClipEnable.depthClipEnable;
    enabledFeatures.extExtendedDynamicState3.extendedDynamicState3RasterizationSamples =
      m_deviceFeatures.extExtendedDynamicState3.extendedDynamicState3RasterizationSamples;
    enabledFeatures.extExtendedDynamicState3.extendedDynamicState3SampleMask =
      m_deviceFeatures.extExtendedDynamicState3.extendedDynamicState3SampleMask;
    enabledFeatures.extExtendedDynamicState3.extendedDynamicState3LineRasterizationMode =
      m_deviceFeatures.extExtendedDynamicState3.extendedDynamicState3LineRasterizationMode;

    // Used for both pNext shader module info, and fast-linking pipelines provided
    // that graphicsPipelineLibraryIndependentInterpolationDecoration is supported
    enabledFeatures.extGraphicsPipelineLibrary.graphicsPipelineLibrary =
      m_deviceFeatures.extGraphicsPipelineLibrary.graphicsPipelineLibrary;

    // Only enable non-default line rasterization features if at least wide lines
    // and rectangular lines are supported. This saves us several feature checks
    // in the actual code.
    if (m_deviceFeatures.core.features.wideLines && m_deviceFeatures.extLineRasterization.rectangularLines) {
      enabledFeatures.core.features.wideLines = VK_TRUE;
      enabledFeatures.extLineRasterization.rectangularLines = VK_TRUE;
      enabledFeatures.extLineRasterization.smoothLines =
        m_deviceFeatures.extLineRasterization.smoothLines;
    }

    // Enable memory priority if supported to improve memory management
    enabledFeatures.extMemoryPriority.memoryPriority =
      m_deviceFeatures.extMemoryPriority.memoryPriority;

    // Require robustBufferAccess2 since we use the robustness alignment
    // info in a number of places, and require null descriptor support
    // since we no longer have a fallback for those in the backend
    enabledFeatures.extRobustness2.robustBufferAccess2 = VK_TRUE;
    enabledFeatures.extRobustness2.robustImageAccess2 = m_deviceFeatures.extRobustness2.robustImageAccess2;
    enabledFeatures.extRobustness2.nullDescriptor = VK_TRUE;

    // We use this to avoid decompressing SPIR-V shaders in some situations
    enabledFeatures.extShaderModuleIdentifier.shaderModuleIdentifier =
      m_deviceFeatures.extShaderModuleIdentifier.shaderModuleIdentifier;

    // Enable swap chain features that are transparent tot he device
    enabledFeatures.extSwapchainMaintenance1.swapchainMaintenance1 =
      m_deviceFeatures.extSwapchainMaintenance1.swapchainMaintenance1 &&
      instance->extensions().extSurfaceMaintenance1;

    // Enable maintenance5 if supported
    enabledFeatures.khrMaintenance5.maintenance5 =
      m_deviceFeatures.khrMaintenance5.maintenance5;

    // Enable present id and present wait together, if possible
    enabledFeatures.khrPresentId.presentId =
      m_deviceFeatures.khrPresentId.presentId;
    enabledFeatures.khrPresentWait.presentWait =
      m_deviceFeatures.khrPresentId.presentId &&
      m_deviceFeatures.khrPresentWait.presentWait;

    // Unless we're on an Nvidia driver where these extensions are known to be broken
    if (matchesDriver(VK_DRIVER_ID_NVIDIA_PROPRIETARY, 0, VK_MAKE_VERSION(535, 0, 0))) {
      enabledFeatures.khrPresentId.presentId = VK_FALSE;
      enabledFeatures.khrPresentWait.presentWait = VK_FALSE;
    }

    // Enable raw access chains for shader backends
    enabledFeatures.nvRawAccessChains.shaderRawAccessChains =
      m_deviceFeatures.nvRawAccessChains.shaderRawAccessChains;

    // Create pNext chain for additional device features
    initFeatureChain(enabledFeatures, devExtensions, instance->extensions());

    // Log feature support info an extension list
    Logger::info(str::format("Device properties:"
      "\n  Device : ", m_deviceInfo.core.properties.deviceName,
      "\n  Driver : ", m_deviceInfo.vk12.driverName, " ",
      VK_VERSION_MAJOR(m_deviceInfo.core.properties.driverVersion), ".",
      VK_VERSION_MINOR(m_deviceInfo.core.properties.driverVersion), ".",
      VK_VERSION_PATCH(m_deviceInfo.core.properties.driverVersion)));

    Logger::info("Enabled device extensions:");
    this->logNameList(extensionNameList);
    this->logFeatures(enabledFeatures);

    // Report the desired overallocation behaviour to the driver
    VkDeviceMemoryOverallocationCreateInfoAMD overallocInfo = { VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD };
    overallocInfo.overallocationBehavior = VK_MEMORY_OVERALLOCATION_BEHAVIOR_ALLOWED_AMD;

    // Create the requested queues
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    std::unordered_set<uint32_t> queueFamiliySet;

    DxvkAdapterQueueIndices queueFamilies = findQueueFamilies();
    queueFamiliySet.insert(queueFamilies.graphics);
    queueFamiliySet.insert(queueFamilies.transfer);

    if (queueFamilies.sparse != VK_QUEUE_FAMILY_IGNORED)
      queueFamiliySet.insert(queueFamilies.sparse);

    this->logQueueFamilies(queueFamilies);
    
    for (uint32_t family : queueFamiliySet) {
      VkDeviceQueueCreateInfo graphicsQueue = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
      graphicsQueue.queueFamilyIndex  = family;
      graphicsQueue.queueCount        = 1;
      graphicsQueue.pQueuePriorities  = &queuePriority;
      queueInfos.push_back(graphicsQueue);
    }

    VkDeviceCreateInfo info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, enabledFeatures.core.pNext };
    info.queueCreateInfoCount       = queueInfos.size();
    info.pQueueCreateInfos          = queueInfos.data();
    info.enabledExtensionCount      = extensionNameList.count();
    info.ppEnabledExtensionNames    = extensionNameList.names();
    info.pEnabledFeatures           = &enabledFeatures.core.features;

    if (devExtensions.amdMemoryOverallocationBehaviour)
      overallocInfo.pNext = std::exchange(info.pNext, &overallocInfo);
    
    VkDevice device = VK_NULL_HANDLE;
    VkResult vr = m_vki->vkCreateDevice(m_handle, &info, nullptr, &device);

    if (vr != VK_SUCCESS && enableCudaInterop) {
      // Enabling certain Vulkan extensions can cause device creation to fail on
      // Nvidia drivers if a certain kernel module isn't loaded, but we cannot know
      // that in advance since the extensions are reported as supported anyway.
      Logger::err("DxvkAdapter: Failed to create device, retrying without CUDA interop extensions");

      extensionsEnabled.disableExtension(devExtensions.nvxBinaryImport);
      extensionsEnabled.disableExtension(devExtensions.nvxImageViewHandle);

      enabledFeatures.vk12.bufferDeviceAddress = VK_FALSE;

      extensionNameList = extensionsEnabled.toNameList();
      info.enabledExtensionCount      = extensionNameList.count();
      info.ppEnabledExtensionNames    = extensionNameList.names();

      vr = m_vki->vkCreateDevice(m_handle, &info, nullptr, &device);
    }

    if (vr != VK_SUCCESS)
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    Rc<vk::DeviceFn> vkd = new vk::DeviceFn(m_vki, true, device);

    DxvkDeviceQueueSet queues = { };
    queues.graphics = getDeviceQueue(vkd, queueFamilies.graphics, 0);
    queues.transfer = getDeviceQueue(vkd, queueFamilies.transfer, 0);
    queues.sparse = getDeviceQueue(vkd, queueFamilies.sparse, 0);

    return new DxvkDevice(instance, this, vkd, enabledFeatures, queues, DxvkQueueCallback());
  }


  Rc<DxvkDevice> DxvkAdapter::importDevice(
    const Rc<DxvkInstance>&   instance,
    const DxvkDeviceImportInfo& args) {
    DxvkDeviceExtensions devExtensions;
    auto devExtensionList = getExtensionList(devExtensions);

    if (!m_deviceExtensions.enableExtensions(devExtensionList.size(), devExtensionList.data(), nullptr))
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    DxvkNameList extensionNameList(args.extensionCount, args.extensionNames);

    // Populate feature structs based on imported Vulkan device
    DxvkDeviceFeatures enabledFeatures = { };

    for (auto f = reinterpret_cast<const VkBaseOutStructure*>(args.features); f; f = f->pNext) {
      switch (f->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
          enabledFeatures.core.features = reinterpret_cast<const VkPhysicalDeviceFeatures2*>(f)->features;
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
          enabledFeatures.vk11 = *reinterpret_cast<const VkPhysicalDeviceVulkan11Features*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
          enabledFeatures.vk12 = *reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
          enabledFeatures.vk13 = *reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT:
          enabledFeatures.extAttachmentFeedbackLoopLayout = *reinterpret_cast<const VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT:
          enabledFeatures.extCustomBorderColor = *reinterpret_cast<const VkPhysicalDeviceCustomBorderColorFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT:
          enabledFeatures.extDepthClipEnable = *reinterpret_cast<const VkPhysicalDeviceDepthClipEnableFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT:
          enabledFeatures.extDepthBiasControl = *reinterpret_cast<const VkPhysicalDeviceDepthBiasControlFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT:
          enabledFeatures.extExtendedDynamicState3 = *reinterpret_cast<const VkPhysicalDeviceExtendedDynamicState3FeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT:
          enabledFeatures.extFragmentShaderInterlock = *reinterpret_cast<const VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT:
          enabledFeatures.extGraphicsPipelineLibrary = *reinterpret_cast<const VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT:
          enabledFeatures.extLineRasterization = *reinterpret_cast<const VkPhysicalDeviceLineRasterizationFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT:
          enabledFeatures.extMemoryPriority = *reinterpret_cast<const VkPhysicalDeviceMemoryPriorityFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT:
          enabledFeatures.extNonSeamlessCubeMap = *reinterpret_cast<const VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT:
          enabledFeatures.extRobustness2 = *reinterpret_cast<const VkPhysicalDeviceRobustness2FeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT:
          enabledFeatures.extShaderModuleIdentifier = *reinterpret_cast<const VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT:
          enabledFeatures.extSwapchainMaintenance1 = *reinterpret_cast<const VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT:
          enabledFeatures.extTransformFeedback = *reinterpret_cast<const VkPhysicalDeviceTransformFeedbackFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT:
          enabledFeatures.extVertexAttributeDivisor = *reinterpret_cast<const VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR:
          enabledFeatures.khrMaintenance5 = *reinterpret_cast<const VkPhysicalDeviceMaintenance5FeaturesKHR*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR:
          enabledFeatures.khrPresentId = *reinterpret_cast<const VkPhysicalDevicePresentIdFeaturesKHR*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR:
          enabledFeatures.khrPresentWait = *reinterpret_cast<const VkPhysicalDevicePresentWaitFeaturesKHR*>(f);
          break;

        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV:
          enabledFeatures.nvRawAccessChains = *reinterpret_cast<const VkPhysicalDeviceRawAccessChainsFeaturesNV*>(f);
          break;

        default:
          // Ignore any unknown feature structs
          break;
      }
    }

    initFeatureChain(enabledFeatures, devExtensions, instance->extensions());

    // Log feature support info an extension list
    Logger::info(str::format("Device properties:"
      "\n  Device name: ", m_deviceInfo.core.properties.deviceName,
      "\n  Driver:      ", m_deviceInfo.vk12.driverName, " ",
      VK_VERSION_MAJOR(m_deviceInfo.core.properties.driverVersion), ".",
      VK_VERSION_MINOR(m_deviceInfo.core.properties.driverVersion), ".",
      VK_VERSION_PATCH(m_deviceInfo.core.properties.driverVersion)));

    Logger::info("Enabled device extensions:");
    this->logNameList(extensionNameList);
    this->logFeatures(enabledFeatures);

    // Create device loader
    Rc<vk::DeviceFn> vkd = new vk::DeviceFn(m_vki, false, args.device);

    // We only support one queue when importing devices, and no sparse.
    DxvkDeviceQueueSet queues = { };
    queues.graphics = { args.queue, args.queueFamily };
    queues.transfer = queues.graphics;

    return new DxvkDevice(instance, this, vkd, enabledFeatures, queues, args.queueCallback);
  }


  void DxvkAdapter::notifyMemoryAlloc(
          uint32_t            heap,
          int64_t             bytes) {
    if (heap < m_memoryAllocated.size())
      m_memoryAllocated[heap] += bytes;
  }


  void DxvkAdapter::notifyMemoryUse(
          uint32_t            heap,
          int64_t             bytes) {
    if (heap < m_memoryUsed.size())
      m_memoryUsed[heap] += bytes;
  }


  bool DxvkAdapter::matchesDriver(
          VkDriverIdKHR       driver,
          uint32_t            minVer,
          uint32_t            maxVer) const {
    bool driverMatches = driver == m_deviceInfo.vk12.driverID;

    if (minVer) driverMatches &= m_deviceInfo.core.properties.driverVersion >= minVer;
    if (maxVer) driverMatches &= m_deviceInfo.core.properties.driverVersion <  maxVer;

    return driverMatches;
  }
  
  
  void DxvkAdapter::logAdapterInfo() const {
    const auto deviceInfo = this->devicePropertiesExt();
    const auto memoryInfo = this->memoryProperties();
    
    Logger::info(str::format(deviceInfo.core.properties.deviceName, ":",
      "\n  Driver : ", deviceInfo.vk12.driverName, " ",
      VK_VERSION_MAJOR(deviceInfo.core.properties.driverVersion), ".",
      VK_VERSION_MINOR(deviceInfo.core.properties.driverVersion), ".",
      VK_VERSION_PATCH(deviceInfo.core.properties.driverVersion)));

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
  
  
  bool DxvkAdapter::isUnifiedMemoryArchitecture() const {
    auto memory = this->memoryProperties();
    bool result = true;

    for (uint32_t i = 0; i < memory.memoryHeapCount; i++)
      result &= memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

    return result;
  }
  
  
  void DxvkAdapter::queryExtensions() {
    m_deviceExtensions = DxvkNameSet::enumDeviceExtensions(m_vki, m_handle);
  }


  void DxvkAdapter::queryDeviceInfo() {
    m_deviceInfo = DxvkDeviceInfo();
    m_deviceInfo.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    m_deviceInfo.core.pNext = nullptr;

    // Query info now so that we have basic device properties available
    m_vki->vkGetPhysicalDeviceProperties2(m_handle, &m_deviceInfo.core);

    m_deviceInfo.vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    m_deviceInfo.vk11.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.vk11);

    m_deviceInfo.vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    m_deviceInfo.vk12.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.vk12);

    m_deviceInfo.vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
    m_deviceInfo.vk13.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.vk13);

    if (m_deviceExtensions.supports(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME)) {
      m_deviceInfo.extConservativeRasterization.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT;
      m_deviceInfo.extConservativeRasterization.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extConservativeRasterization);
    }

    if (m_deviceExtensions.supports(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME)) {
      m_deviceInfo.extCustomBorderColor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT;
      m_deviceInfo.extCustomBorderColor.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extCustomBorderColor);
    }

    if (m_deviceExtensions.supports(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)) {
      m_deviceInfo.extExtendedDynamicState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT;
      m_deviceInfo.extExtendedDynamicState3.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extExtendedDynamicState3);
    }

    if (m_deviceExtensions.supports(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)) {
      m_deviceInfo.extGraphicsPipelineLibrary.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
      m_deviceInfo.extGraphicsPipelineLibrary.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extGraphicsPipelineLibrary);
    }

    if (m_deviceExtensions.supports(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME)) {
      m_deviceInfo.extLineRasterization.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT;
      m_deviceInfo.extLineRasterization.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extLineRasterization);
    }

    if (m_deviceExtensions.supports(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
      m_deviceInfo.extRobustness2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT;
      m_deviceInfo.extRobustness2.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extRobustness2);
    }

    if (m_deviceExtensions.supports(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)) {
      m_deviceInfo.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
      m_deviceInfo.extTransformFeedback.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extTransformFeedback);
    }

    if (m_deviceExtensions.supports(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME)) {
      m_deviceInfo.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
      m_deviceInfo.extVertexAttributeDivisor.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extVertexAttributeDivisor);
    }

    if (m_deviceExtensions.supports(VK_KHR_MAINTENANCE_5_EXTENSION_NAME)) {
      m_deviceInfo.khrMaintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR;
      m_deviceInfo.khrMaintenance5.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.khrMaintenance5);
    }

    // Query full device properties for all enabled extensions
    m_vki->vkGetPhysicalDeviceProperties2(m_handle, &m_deviceInfo.core);
    
    // Some drivers reports the driver version in a slightly different format
    switch (m_deviceInfo.vk12.driverID) {
      case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
        m_deviceInfo.core.properties.driverVersion = VK_MAKE_VERSION(
          (m_deviceInfo.core.properties.driverVersion >> 22) & 0x3ff,
          (m_deviceInfo.core.properties.driverVersion >> 14) & 0x0ff,
          (m_deviceInfo.core.properties.driverVersion >>  6) & 0x0ff);
        break;

      case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
        m_deviceInfo.core.properties.driverVersion = VK_MAKE_VERSION(
          m_deviceInfo.core.properties.driverVersion >> 14,
          m_deviceInfo.core.properties.driverVersion & 0x3fff, 0);
        break;

      default:;
    }
  }


  void DxvkAdapter::queryDeviceFeatures() {
    m_deviceFeatures = DxvkDeviceFeatures();
    m_deviceFeatures.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    m_deviceFeatures.core.pNext = nullptr;

    m_deviceFeatures.vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    m_deviceFeatures.vk11.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.vk11);

    m_deviceFeatures.vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    m_deviceFeatures.vk12.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.vk12);

    m_deviceFeatures.vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    m_deviceFeatures.vk13.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.vk13);

    if (m_deviceExtensions.supports(VK_AMD_SHADER_FRAGMENT_MASK_EXTENSION_NAME))
      m_deviceFeatures.amdShaderFragmentMask = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME)) {
      m_deviceFeatures.extAttachmentFeedbackLoopLayout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT;
      m_deviceFeatures.extAttachmentFeedbackLoopLayout.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extAttachmentFeedbackLoopLayout);
    }

    if (m_deviceExtensions.supports(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME))
      m_deviceFeatures.extConservativeRasterization = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME)) {
      m_deviceFeatures.extCustomBorderColor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
      m_deviceFeatures.extCustomBorderColor.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extCustomBorderColor);
    }

    if (m_deviceExtensions.supports(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME)) {
      m_deviceFeatures.extDepthClipEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
      m_deviceFeatures.extDepthClipEnable.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extDepthClipEnable);
    }

    if (m_deviceExtensions.supports(VK_EXT_DEPTH_BIAS_CONTROL_EXTENSION_NAME)) {
      m_deviceFeatures.extDepthBiasControl.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT;
      m_deviceFeatures.extDepthBiasControl.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extDepthBiasControl);
    }

    if (m_deviceExtensions.supports(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME)) {
      m_deviceFeatures.extExtendedDynamicState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
      m_deviceFeatures.extExtendedDynamicState3.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extExtendedDynamicState3);
    }

    if (m_deviceExtensions.supports(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)) {
      m_deviceFeatures.extFragmentShaderInterlock.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
      m_deviceFeatures.extFragmentShaderInterlock.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extFragmentShaderInterlock);
    }

    if (m_deviceExtensions.supports(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME))
      m_deviceFeatures.extFullScreenExclusive = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)) {
      m_deviceFeatures.extGraphicsPipelineLibrary.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
      m_deviceFeatures.extGraphicsPipelineLibrary.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extGraphicsPipelineLibrary);
    }

    if (m_deviceExtensions.supports(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME)) {
      m_deviceFeatures.extLineRasterization.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
      m_deviceFeatures.extLineRasterization.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extLineRasterization);
    }

    if (m_deviceExtensions.supports(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
      m_deviceFeatures.extMemoryBudget = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
      m_deviceFeatures.extMemoryPriority.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
      m_deviceFeatures.extMemoryPriority.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extMemoryPriority);
    }

    if (m_deviceExtensions.supports(VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME)) {
      m_deviceFeatures.extNonSeamlessCubeMap.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT;
      m_deviceFeatures.extNonSeamlessCubeMap.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extNonSeamlessCubeMap);
    }

    if (m_deviceExtensions.supports(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
      m_deviceFeatures.extRobustness2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
      m_deviceFeatures.extRobustness2.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extRobustness2);
    }

    if (m_deviceExtensions.supports(VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME)) {
      m_deviceFeatures.extShaderModuleIdentifier.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT;
      m_deviceFeatures.extShaderModuleIdentifier.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extShaderModuleIdentifier);
    }

    if (m_deviceExtensions.supports(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME))
      m_deviceFeatures.extShaderStencilExport = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME))
      m_deviceFeatures.extSwapchainColorSpace = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
      m_deviceFeatures.extSwapchainMaintenance1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
      m_deviceFeatures.extSwapchainMaintenance1.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extSwapchainMaintenance1);
    }

    if (m_deviceExtensions.supports(VK_EXT_HDR_METADATA_EXTENSION_NAME))
      m_deviceFeatures.extHdrMetadata = VK_TRUE;

    if (m_deviceExtensions.supports(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)) {
      m_deviceFeatures.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
      m_deviceFeatures.extTransformFeedback.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extTransformFeedback);
    }

    if (m_deviceExtensions.supports(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME) >= 3) {
      m_deviceFeatures.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
      m_deviceFeatures.extVertexAttributeDivisor.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extVertexAttributeDivisor);
    }

    if (m_deviceExtensions.supports(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME))
      m_deviceFeatures.khrExternalMemoryWin32 = VK_TRUE;

    if (m_deviceExtensions.supports(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME))
      m_deviceFeatures.khrExternalSemaphoreWin32 = VK_TRUE;

    if (m_deviceExtensions.supports(VK_KHR_MAINTENANCE_5_EXTENSION_NAME)) {
      m_deviceFeatures.khrMaintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
      m_deviceFeatures.khrMaintenance5.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.khrMaintenance5);
    }

    if (m_deviceExtensions.supports(VK_KHR_PRESENT_ID_EXTENSION_NAME)) {
      m_deviceFeatures.khrPresentId.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
      m_deviceFeatures.khrPresentId.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.khrPresentId);
    }

    if (m_deviceExtensions.supports(VK_KHR_PRESENT_WAIT_EXTENSION_NAME)) {
      m_deviceFeatures.khrPresentWait.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
      m_deviceFeatures.khrPresentWait.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.khrPresentWait);
    }

    if (m_deviceExtensions.supports(VK_NV_RAW_ACCESS_CHAINS_EXTENSION_NAME)) {
      m_deviceFeatures.nvRawAccessChains.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV;
      m_deviceFeatures.nvRawAccessChains.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.nvRawAccessChains);
    }

    if (m_deviceExtensions.supports(VK_NVX_BINARY_IMPORT_EXTENSION_NAME))
      m_deviceFeatures.nvxBinaryImport = VK_TRUE;

    if (m_deviceExtensions.supports(VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME))
      m_deviceFeatures.nvxImageViewHandle = VK_TRUE;

    m_vki->vkGetPhysicalDeviceFeatures2(m_handle, &m_deviceFeatures.core);
  }


  void DxvkAdapter::queryDeviceQueues() {
    uint32_t numQueueFamilies = 0;
    m_vki->vkGetPhysicalDeviceQueueFamilyProperties(
      m_handle, &numQueueFamilies, nullptr);
    
    m_queueFamilies.resize(numQueueFamilies);
    m_vki->vkGetPhysicalDeviceQueueFamilyProperties(
      m_handle, &numQueueFamilies, m_queueFamilies.data());
  }


  uint32_t DxvkAdapter::findQueueFamily(
          VkQueueFlags          mask,
          VkQueueFlags          flags) const {
    for (uint32_t i = 0; i < m_queueFamilies.size(); i++) {
      if ((m_queueFamilies[i].queueFlags & mask) == flags)
        return i;
    }

    return VK_QUEUE_FAMILY_IGNORED;
  }


  std::vector<DxvkExt*> DxvkAdapter::getExtensionList(
          DxvkDeviceExtensions&   devExtensions) {
    return {{
      &devExtensions.amdMemoryOverallocationBehaviour,
      &devExtensions.amdShaderFragmentMask,
      &devExtensions.extAttachmentFeedbackLoopLayout,
      &devExtensions.extConservativeRasterization,
      &devExtensions.extCustomBorderColor,
      &devExtensions.extDepthClipEnable,
      &devExtensions.extDepthBiasControl,
      &devExtensions.extExtendedDynamicState3,
      &devExtensions.extFragmentShaderInterlock,
      &devExtensions.extFullScreenExclusive,
      &devExtensions.extGraphicsPipelineLibrary,
      &devExtensions.extHdrMetadata,
      &devExtensions.extLineRasterization,
      &devExtensions.extMemoryBudget,
      &devExtensions.extMemoryPriority,
      &devExtensions.extNonSeamlessCubeMap,
      &devExtensions.extRobustness2,
      &devExtensions.extShaderModuleIdentifier,
      &devExtensions.extShaderStencilExport,
      &devExtensions.extSwapchainColorSpace,
      &devExtensions.extSwapchainMaintenance1,
      &devExtensions.extTransformFeedback,
      &devExtensions.extVertexAttributeDivisor,
      &devExtensions.khrExternalMemoryWin32,
      &devExtensions.khrExternalSemaphoreWin32,
      &devExtensions.khrMaintenance5,
      &devExtensions.khrPipelineLibrary,
      &devExtensions.khrPresentId,
      &devExtensions.khrPresentWait,
      &devExtensions.khrSwapchain,
      &devExtensions.khrWin32KeyedMutex,
      &devExtensions.nvRawAccessChains,
      &devExtensions.nvxBinaryImport,
      &devExtensions.nvxImageViewHandle,
    }};
  }


  void DxvkAdapter::initFeatureChain(
          DxvkDeviceFeatures&   enabledFeatures,
    const DxvkDeviceExtensions& devExtensions,
    const DxvkInstanceExtensions& insExtensions) {
    enabledFeatures.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    enabledFeatures.core.pNext = nullptr;

    enabledFeatures.vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    enabledFeatures.vk11.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.vk11);

    enabledFeatures.vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabledFeatures.vk12.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.vk12);

    enabledFeatures.vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enabledFeatures.vk13.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.vk13);

    if (devExtensions.amdShaderFragmentMask)
      enabledFeatures.amdShaderFragmentMask = VK_TRUE;

    if (devExtensions.extAttachmentFeedbackLoopLayout) {
      enabledFeatures.extAttachmentFeedbackLoopLayout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT;
      enabledFeatures.extAttachmentFeedbackLoopLayout.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extAttachmentFeedbackLoopLayout);
    }

    if (devExtensions.extConservativeRasterization)
      enabledFeatures.extConservativeRasterization = VK_TRUE;

    if (devExtensions.extCustomBorderColor) {
      enabledFeatures.extCustomBorderColor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
      enabledFeatures.extCustomBorderColor.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extCustomBorderColor);
    }

    if (devExtensions.extDepthClipEnable) {
      enabledFeatures.extDepthClipEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
      enabledFeatures.extDepthClipEnable.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extDepthClipEnable);
    }

    if (devExtensions.extDepthBiasControl) {
      enabledFeatures.extDepthBiasControl.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT;
      enabledFeatures.extDepthBiasControl.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extDepthBiasControl);
    }

    if (devExtensions.extExtendedDynamicState3) {
      enabledFeatures.extExtendedDynamicState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
      enabledFeatures.extExtendedDynamicState3.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extExtendedDynamicState3);
    }

    if (devExtensions.extFragmentShaderInterlock) {
      enabledFeatures.extFragmentShaderInterlock.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
      enabledFeatures.extFragmentShaderInterlock.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extFragmentShaderInterlock);
    }

    if (devExtensions.extFullScreenExclusive && insExtensions.khrGetSurfaceCapabilities2)
      enabledFeatures.extFullScreenExclusive = VK_TRUE;

    if (devExtensions.extGraphicsPipelineLibrary) {
      enabledFeatures.extGraphicsPipelineLibrary.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
      enabledFeatures.extGraphicsPipelineLibrary.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extGraphicsPipelineLibrary);
    }

    if (devExtensions.extLineRasterization) {
      enabledFeatures.extLineRasterization.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
      enabledFeatures.extLineRasterization.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extLineRasterization);
    }

    if (devExtensions.extMemoryBudget)
      enabledFeatures.extMemoryBudget = VK_TRUE;

    if (devExtensions.extMemoryPriority) {
      enabledFeatures.extMemoryPriority.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
      enabledFeatures.extMemoryPriority.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extMemoryPriority);
    }

    if (devExtensions.extNonSeamlessCubeMap) {
      enabledFeatures.extNonSeamlessCubeMap.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT;
      enabledFeatures.extNonSeamlessCubeMap.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extNonSeamlessCubeMap);
    }

    if (devExtensions.extShaderModuleIdentifier) {
      enabledFeatures.extShaderModuleIdentifier.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT;
      enabledFeatures.extShaderModuleIdentifier.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extShaderModuleIdentifier);
    }

    if (devExtensions.extRobustness2) {
      enabledFeatures.extRobustness2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
      enabledFeatures.extRobustness2.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extRobustness2);
    }

    if (devExtensions.extShaderStencilExport)
      enabledFeatures.extShaderStencilExport = VK_TRUE;

    if (devExtensions.extSwapchainColorSpace)
      enabledFeatures.extSwapchainColorSpace = VK_TRUE;

    if (devExtensions.extSwapchainMaintenance1) {
      enabledFeatures.extSwapchainMaintenance1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
      enabledFeatures.extSwapchainMaintenance1.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extSwapchainMaintenance1);
    }

    if (devExtensions.extHdrMetadata)
      enabledFeatures.extHdrMetadata = VK_TRUE;

    if (devExtensions.extTransformFeedback) {
      enabledFeatures.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
      enabledFeatures.extTransformFeedback.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extTransformFeedback);
    }

    if (devExtensions.extVertexAttributeDivisor.revision() >= 3) {
      enabledFeatures.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
      enabledFeatures.extVertexAttributeDivisor.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extVertexAttributeDivisor);
    }

    if (devExtensions.khrExternalMemoryWin32)
      enabledFeatures.khrExternalMemoryWin32 = VK_TRUE;

    if (devExtensions.khrExternalSemaphoreWin32)
      enabledFeatures.khrExternalSemaphoreWin32 = VK_TRUE;

    if (devExtensions.khrMaintenance5) {
      enabledFeatures.khrMaintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
      enabledFeatures.khrMaintenance5.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.khrMaintenance5);
    }

    if (devExtensions.khrPresentId) {
      enabledFeatures.khrPresentId.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
      enabledFeatures.khrPresentId.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.khrPresentId);
    }

    if (devExtensions.khrPresentWait) {
      enabledFeatures.khrPresentWait.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
      enabledFeatures.khrPresentWait.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.khrPresentWait);
    }

    if (devExtensions.nvRawAccessChains) {
      enabledFeatures.nvRawAccessChains.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV;
      enabledFeatures.nvRawAccessChains.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.nvRawAccessChains);
    }

    if (devExtensions.nvxBinaryImport)
      enabledFeatures.nvxBinaryImport = VK_TRUE;

    if (devExtensions.nvxImageViewHandle)
      enabledFeatures.nvxImageViewHandle = VK_TRUE;

    if (devExtensions.khrWin32KeyedMutex)
      enabledFeatures.khrWin32KeyedMutex = VK_TRUE;
  }

  
  void DxvkAdapter::logNameList(const DxvkNameList& names) {
    for (uint32_t i = 0; i < names.count(); i++)
      Logger::info(str::format("  ", names.name(i)));
  }


  void DxvkAdapter::logFeatures(const DxvkDeviceFeatures& features) {
    Logger::info(str::format("Device features:",
      "\n  robustBufferAccess                     : ", features.core.features.robustBufferAccess ? "1" : "0",
      "\n  fullDrawIndexUint32                    : ", features.core.features.fullDrawIndexUint32 ? "1" : "0",
      "\n  imageCubeArray                         : ", features.core.features.imageCubeArray ? "1" : "0",
      "\n  independentBlend                       : ", features.core.features.independentBlend ? "1" : "0",
      "\n  geometryShader                         : ", features.core.features.geometryShader ? "1" : "0",
      "\n  tessellationShader                     : ", features.core.features.tessellationShader ? "1" : "0",
      "\n  sampleRateShading                      : ", features.core.features.sampleRateShading ? "1" : "0",
      "\n  dualSrcBlend                           : ", features.core.features.dualSrcBlend ? "1" : "0",
      "\n  logicOp                                : ", features.core.features.logicOp ? "1" : "0",
      "\n  multiDrawIndirect                      : ", features.core.features.multiDrawIndirect ? "1" : "0",
      "\n  drawIndirectFirstInstance              : ", features.core.features.drawIndirectFirstInstance ? "1" : "0",
      "\n  depthClamp                             : ", features.core.features.depthClamp ? "1" : "0",
      "\n  depthBiasClamp                         : ", features.core.features.depthBiasClamp ? "1" : "0",
      "\n  fillModeNonSolid                       : ", features.core.features.fillModeNonSolid ? "1" : "0",
      "\n  depthBounds                            : ", features.core.features.depthBounds ? "1" : "0",
      "\n  wideLines                              : ", features.core.features.wideLines ? "1" : "0",
      "\n  multiViewport                          : ", features.core.features.multiViewport ? "1" : "0",
      "\n  samplerAnisotropy                      : ", features.core.features.samplerAnisotropy ? "1" : "0",
      "\n  textureCompressionBC                   : ", features.core.features.textureCompressionBC ? "1" : "0",
      "\n  occlusionQueryPrecise                  : ", features.core.features.occlusionQueryPrecise ? "1" : "0",
      "\n  pipelineStatisticsQuery                : ", features.core.features.pipelineStatisticsQuery ? "1" : "0",
      "\n  vertexPipelineStoresAndAtomics         : ", features.core.features.vertexPipelineStoresAndAtomics ? "1" : "0",
      "\n  fragmentStoresAndAtomics               : ", features.core.features.fragmentStoresAndAtomics ? "1" : "0",
      "\n  shaderImageGatherExtended              : ", features.core.features.shaderImageGatherExtended ? "1" : "0",
      "\n  shaderClipDistance                     : ", features.core.features.shaderClipDistance ? "1" : "0",
      "\n  shaderCullDistance                     : ", features.core.features.shaderCullDistance ? "1" : "0",
      "\n  shaderFloat64                          : ", features.core.features.shaderFloat64 ? "1" : "0",
      "\n  shaderInt64                            : ", features.core.features.shaderInt64 ? "1" : "0",
      "\n  variableMultisampleRate                : ", features.core.features.variableMultisampleRate ? "1" : "0",
      "\n  shaderResourceResidency                : ", features.core.features.shaderResourceResidency ? "1" : "0",
      "\n  shaderResourceMinLod                   : ", features.core.features.shaderResourceMinLod ? "1" : "0",
      "\n  sparseBinding                          : ", features.core.features.sparseBinding ? "1" : "0",
      "\n  sparseResidencyBuffer                  : ", features.core.features.sparseResidencyBuffer ? "1" : "0",
      "\n  sparseResidencyImage2D                 : ", features.core.features.sparseResidencyImage2D ? "1" : "0",
      "\n  sparseResidencyImage3D                 : ", features.core.features.sparseResidencyImage3D ? "1" : "0",
      "\n  sparseResidency2Samples                : ", features.core.features.sparseResidency2Samples ? "1" : "0",
      "\n  sparseResidency4Samples                : ", features.core.features.sparseResidency4Samples ? "1" : "0",
      "\n  sparseResidency8Samples                : ", features.core.features.sparseResidency8Samples ? "1" : "0",
      "\n  sparseResidency16Samples               : ", features.core.features.sparseResidency16Samples ? "1" : "0",
      "\n  sparseResidencyAliased                 : ", features.core.features.sparseResidencyAliased ? "1" : "0",
      "\nVulkan 1.1",
      "\n  shaderDrawParameters                   : ", features.vk11.shaderDrawParameters,
      "\nVulkan 1.2",
      "\n  samplerMirrorClampToEdge               : ", features.vk12.samplerMirrorClampToEdge,
      "\n  drawIndirectCount                      : ", features.vk12.drawIndirectCount,
      "\n  samplerFilterMinmax                    : ", features.vk12.samplerFilterMinmax,
      "\n  hostQueryReset                         : ", features.vk12.hostQueryReset,
      "\n  timelineSemaphore                      : ", features.vk12.timelineSemaphore,
      "\n  bufferDeviceAddress                    : ", features.vk12.bufferDeviceAddress,
      "\n  shaderOutputViewportIndex              : ", features.vk12.shaderOutputViewportIndex,
      "\n  shaderOutputLayer                      : ", features.vk12.shaderOutputLayer,
      "\n  vulkanMemoryModel                      : ", features.vk12.vulkanMemoryModel,
      "\nVulkan 1.3",
      "\n  robustImageAccess                      : ", features.vk13.robustImageAccess,
      "\n  pipelineCreationCacheControl           : ", features.vk13.pipelineCreationCacheControl,
      "\n  shaderDemoteToHelperInvocation         : ", features.vk13.shaderDemoteToHelperInvocation,
      "\n  shaderZeroInitializeWorkgroupMemory    : ", features.vk13.shaderZeroInitializeWorkgroupMemory,
      "\n  synchronization2                       : ", features.vk13.synchronization2,
      "\n  dynamicRendering                       : ", features.vk13.dynamicRendering,
      "\n", VK_AMD_SHADER_FRAGMENT_MASK_EXTENSION_NAME,
      "\n  extension supported                    : ", features.amdShaderFragmentMask ? "1" : "0",
      "\n", VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME,
      "\n  attachmentFeedbackLoopLayout           : ", features.extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout ? "1" : "0",
      "\n", VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
      "\n  extension supported                    : ", features.extConservativeRasterization ? "1" : "0",
      "\n", VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME,
      "\n  customBorderColors                     : ", features.extCustomBorderColor.customBorderColors ? "1" : "0",
      "\n  customBorderColorWithoutFormat         : ", features.extCustomBorderColor.customBorderColorWithoutFormat ? "1" : "0",
      "\n", VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
      "\n  depthClipEnable                        : ", features.extDepthClipEnable.depthClipEnable ? "1" : "0",
      "\n", VK_EXT_DEPTH_BIAS_CONTROL_EXTENSION_NAME,
      "\n  depthBiasControl                       : ", features.extDepthBiasControl.depthBiasControl ? "1" : "0",
      "\n  leastRepresentableValueForceUnormRepresentation : ", features.extDepthBiasControl.leastRepresentableValueForceUnormRepresentation ? "1" : "0",
      "\n  floatRepresentation                    : ", features.extDepthBiasControl.floatRepresentation ? "1" : "0",
      "\n  depthBiasExact                         : ", features.extDepthBiasControl.depthBiasExact ? "1" : "0",
      "\n", VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
      "\n  extDynamicState3AlphaToCoverageEnable  : ", features.extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable ? "1" : "0",
      "\n  extDynamicState3DepthClipEnable        : ", features.extExtendedDynamicState3.extendedDynamicState3DepthClipEnable ? "1" : "0",
      "\n  extDynamicState3RasterizationSamples   : ", features.extExtendedDynamicState3.extendedDynamicState3RasterizationSamples ? "1" : "0",
      "\n  extDynamicState3SampleMask             : ", features.extExtendedDynamicState3.extendedDynamicState3SampleMask ? "1" : "0",
      "\n  extDynamicState3LineRasterizationMode  : ", features.extExtendedDynamicState3.extendedDynamicState3LineRasterizationMode ? "1" : "0",
      "\n", VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
      "\n  fragmentShaderSampleInterlock          : ", features.extFragmentShaderInterlock.fragmentShaderSampleInterlock ? "1" : "0",
      "\n  fragmentShaderPixelInterlock           : ", features.extFragmentShaderInterlock.fragmentShaderPixelInterlock ? "1" : "0",
      "\n", VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
      "\n  extension supported                    : ", features.extFullScreenExclusive ? "1" : "0",
      "\n", VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
      "\n  graphicsPipelineLibrary                : ", features.extGraphicsPipelineLibrary.graphicsPipelineLibrary ? "1" : "0",
      "\n", VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME,
      "\n  rectangularLines                       : ", features.extLineRasterization.rectangularLines ? "1" : "0",
      "\n  smoothLines                            : ", features.extLineRasterization.smoothLines ? "1" : "0",
      "\n", VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
      "\n  extension supported                    : ", features.extMemoryBudget ? "1" : "0",
      "\n", VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
      "\n  memoryPriority                         : ", features.extMemoryPriority.memoryPriority ? "1" : "0",
      "\n", VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME,
      "\n  nonSeamlessCubeMap                     : ", features.extNonSeamlessCubeMap.nonSeamlessCubeMap ? "1" : "0",
      "\n", VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
      "\n  robustBufferAccess2                    : ", features.extRobustness2.robustBufferAccess2 ? "1" : "0",
      "\n  robustImageAccess2                     : ", features.extRobustness2.robustImageAccess2 ? "1" : "0",
      "\n  nullDescriptor                         : ", features.extRobustness2.nullDescriptor ? "1" : "0",
      "\n", VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME,
      "\n  shaderModuleIdentifier                 : ", features.extShaderModuleIdentifier.shaderModuleIdentifier ? "1" : "0",
      "\n", VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME,
      "\n  extension supported                    : ", features.extShaderStencilExport ? "1" : "0",
      "\n", VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
      "\n  extension supported                    : ", features.extSwapchainColorSpace ? "1" : "0",
      "\n", VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
      "\n  swapchainMaintenance1                  : ", features.extSwapchainMaintenance1.swapchainMaintenance1 ? "1" : "0",
      "\n", VK_EXT_HDR_METADATA_EXTENSION_NAME,
      "\n  extension supported                    : ", features.extHdrMetadata ? "1" : "0",
      "\n", VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME,
      "\n  transformFeedback                      : ", features.extTransformFeedback.transformFeedback ? "1" : "0",
      "\n  geometryStreams                        : ", features.extTransformFeedback.geometryStreams ? "1" : "0",
      "\n", VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
      "\n  vertexAttributeInstanceRateDivisor     : ", features.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor ? "1" : "0",
      "\n  vertexAttributeInstanceRateZeroDivisor : ", features.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor ? "1" : "0",
      "\n", VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
      "\n  extension supported                    : ", features.khrExternalMemoryWin32 ? "1" : "0",
      "\n", VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
      "\n  extension supported                    : ", features.khrExternalSemaphoreWin32 ? "1" : "0",
      "\n", VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
      "\n  maintenance5                           : ", features.khrMaintenance5.maintenance5 ? "1" : "0",
      "\n", VK_KHR_PRESENT_ID_EXTENSION_NAME,
      "\n  presentId                              : ", features.khrPresentId.presentId ? "1" : "0",
      "\n", VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
      "\n  presentWait                            : ", features.khrPresentWait.presentWait ? "1" : "0",
      "\n", VK_NV_RAW_ACCESS_CHAINS_EXTENSION_NAME,
      "\n  shaderRawAccessChains                  : ", features.nvRawAccessChains.shaderRawAccessChains ? "1" : "0",
      "\n", VK_NVX_BINARY_IMPORT_EXTENSION_NAME,
      "\n  extension supported                    : ", features.nvxBinaryImport ? "1" : "0",
      "\n", VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME,
      "\n  extension supported                    : ", features.nvxImageViewHandle ? "1" : "0",
      "\n", VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME,
      "\n  extension supported                    : ", features.khrWin32KeyedMutex ? "1" : "0"));
  }


  void DxvkAdapter::logQueueFamilies(const DxvkAdapterQueueIndices& queues) {
    Logger::info(str::format("Queue families:",
      "\n  Graphics : ", queues.graphics,
      "\n  Transfer : ", queues.transfer,
      "\n  Sparse   : ", queues.sparse != VK_QUEUE_FAMILY_IGNORED ? str::format(queues.sparse) : "n/a"));
  }
  
}
