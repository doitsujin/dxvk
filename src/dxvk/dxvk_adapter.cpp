#include <cstring>
#include <unordered_set>

#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkAdapter::DxvkAdapter(
    const Rc<vk::InstanceFn>& vki,
          VkPhysicalDevice    handle)
  : m_vki           (vki),
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
  
  
  DxvkAdapterMemoryInfo DxvkAdapter::getMemoryHeapInfo() const {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    VkPhysicalDeviceMemoryProperties2 memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    memProps.pNext = m_hasMemoryBudget ? &memBudget : nullptr;

    m_vki->vkGetPhysicalDeviceMemoryProperties2(m_handle, &memProps);
    
    DxvkAdapterMemoryInfo info = { };
    info.heapCount = memProps.memoryProperties.memoryHeapCount;

    for (uint32_t i = 0; i < info.heapCount; i++) {
      info.heaps[i].heapFlags = memProps.memoryProperties.memoryHeaps[i].flags;

      if (m_hasMemoryBudget) {
        info.heaps[i].memoryBudget    = memBudget.heapBudget[i];
        info.heaps[i].memoryAllocated = memBudget.heapUsage[i];
      } else {
        info.heaps[i].memoryBudget    = memProps.memoryProperties.memoryHeaps[i].size;
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
    
    DxvkAdapterQueueIndices queues;
    queues.graphics = graphicsQueue;
    queues.transfer = transferQueue;
    return queues;
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
        && (m_deviceFeatures.vk11.shaderDrawParameters
                || !required.vk11.shaderDrawParameters)
        && (m_deviceFeatures.vk12.samplerMirrorClampToEdge
                || !required.vk12.samplerMirrorClampToEdge)
        && (m_deviceFeatures.vk12.drawIndirectCount
                || !required.vk12.drawIndirectCount)
        && (m_deviceFeatures.vk12.hostQueryReset
                || !required.vk12.hostQueryReset)
        && (m_deviceFeatures.vk12.timelineSemaphore
                || !required.vk12.timelineSemaphore)
        && (m_deviceFeatures.vk12.bufferDeviceAddress
                || !required.vk12.bufferDeviceAddress)
        && (m_deviceFeatures.vk12.shaderOutputViewportIndex
                || !required.vk12.shaderOutputViewportIndex)
        && (m_deviceFeatures.vk12.shaderOutputLayer
                || !required.vk12.shaderOutputLayer)
        && (m_deviceFeatures.vk13.pipelineCreationCacheControl
                || !required.vk13.pipelineCreationCacheControl)
        && (m_deviceFeatures.vk13.shaderDemoteToHelperInvocation
                || !required.vk13.shaderDemoteToHelperInvocation)
        && (m_deviceFeatures.vk13.shaderZeroInitializeWorkgroupMemory
                || !required.vk13.shaderZeroInitializeWorkgroupMemory)
        && (m_deviceFeatures.vk13.synchronization2
                || !required.vk13.synchronization2)
        && (m_deviceFeatures.vk13.dynamicRendering
                || !required.vk13.dynamicRendering)
        && (m_deviceFeatures.vk13.maintenance4
                || !required.vk13.maintenance4)
        && (m_deviceFeatures.extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout
                || !required.extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout)
        && (m_deviceFeatures.extCustomBorderColor.customBorderColors
                || !required.extCustomBorderColor.customBorderColors)
        && (m_deviceFeatures.extCustomBorderColor.customBorderColorWithoutFormat
                || !required.extCustomBorderColor.customBorderColorWithoutFormat)
        && (m_deviceFeatures.extDepthClipEnable.depthClipEnable
                || !required.extDepthClipEnable.depthClipEnable)
        && (m_deviceFeatures.extGraphicsPipelineLibrary.graphicsPipelineLibrary
                || !required.extGraphicsPipelineLibrary.graphicsPipelineLibrary)
        && (m_deviceFeatures.extMemoryPriority.memoryPriority
                || !required.extMemoryPriority.memoryPriority)
        && (m_deviceFeatures.extNonSeamlessCubeMap.nonSeamlessCubeMap
                || !required.extNonSeamlessCubeMap.nonSeamlessCubeMap)
        && (m_deviceFeatures.extRobustness2.robustBufferAccess2
                || !required.extRobustness2.robustBufferAccess2)
        && (m_deviceFeatures.extRobustness2.robustImageAccess2
                || !required.extRobustness2.robustImageAccess2)
        && (m_deviceFeatures.extRobustness2.nullDescriptor
                || !required.extRobustness2.nullDescriptor)
        && (m_deviceFeatures.extShaderModuleIdentifier.shaderModuleIdentifier
                || !required.extShaderModuleIdentifier.shaderModuleIdentifier)
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


  Rc<DxvkDevice> DxvkAdapter::createDevice(
    const Rc<DxvkInstance>&   instance,
          DxvkDeviceFeatures  enabledFeatures) {
    DxvkDeviceExtensions devExtensions;

    std::array<DxvkExt*, 22> devExtensionList = {{
      &devExtensions.amdMemoryOverallocationBehaviour,
      &devExtensions.amdShaderFragmentMask,
      &devExtensions.extAttachmentFeedbackLoopLayout,
      &devExtensions.extConservativeRasterization,
      &devExtensions.extCustomBorderColor,
      &devExtensions.extDepthClipEnable,
      &devExtensions.extFullScreenExclusive,
      &devExtensions.extGraphicsPipelineLibrary,
      &devExtensions.extMemoryBudget,
      &devExtensions.extMemoryPriority,
      &devExtensions.extNonSeamlessCubeMap,
      &devExtensions.extRobustness2,
      &devExtensions.extShaderModuleIdentifier,
      &devExtensions.extShaderStencilExport,
      &devExtensions.extTransformFeedback,
      &devExtensions.extVertexAttributeDivisor,
      &devExtensions.khrExternalMemoryWin32,
      &devExtensions.khrExternalSemaphoreWin32,
      &devExtensions.khrPipelineLibrary,
      &devExtensions.khrSwapchain,
      &devExtensions.nvxBinaryImport,
      &devExtensions.nvxImageViewHandle,
    }};

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
          extensionsEnabled))
      throw DxvkError("DxvkAdapter: Failed to create device");
    
    // Enable additional extensions if necessary
    extensionsEnabled.merge(m_extraExtensions);
    DxvkNameList extensionNameList = extensionsEnabled.toNameList();

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

    // Used for both pNext shader module info, and fast-linking pipelines provided
    // that graphicsPipelineLibraryIndependentInterpolationDecoration is supported
    enabledFeatures.extGraphicsPipelineLibrary.graphicsPipelineLibrary =
      m_deviceFeatures.extGraphicsPipelineLibrary.graphicsPipelineLibrary;

    // Require robustBufferAccess2 since we use the robustness alignment
    // info in a number of places, and require null descriptor support
    // since we no longer have a fallback for those in the backend
    enabledFeatures.extRobustness2.robustBufferAccess2 = VK_TRUE;
    enabledFeatures.extRobustness2.robustImageAccess2 = m_deviceFeatures.extRobustness2.robustImageAccess2;
    enabledFeatures.extRobustness2.nullDescriptor = VK_TRUE;

    // We use this to avoid decompressing SPIR-V shaders in some situations
    enabledFeatures.extShaderModuleIdentifier.shaderModuleIdentifier =
      m_deviceFeatures.extShaderModuleIdentifier.shaderModuleIdentifier;

    Logger::info(str::format("Device properties:"
      "\n  Device name:     : ", m_deviceInfo.core.properties.deviceName,
      "\n  Driver version   : ",
        VK_VERSION_MAJOR(m_deviceInfo.core.properties.driverVersion), ".",
        VK_VERSION_MINOR(m_deviceInfo.core.properties.driverVersion), ".",
        VK_VERSION_PATCH(m_deviceInfo.core.properties.driverVersion)));

    Logger::info("Enabled device extensions:");
    this->logNameList(extensionNameList);
    this->logFeatures(enabledFeatures);

    // Create pNext chain for additional device features
    enabledFeatures.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    enabledFeatures.core.pNext = nullptr;

    enabledFeatures.vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    enabledFeatures.vk11.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.vk11);

    enabledFeatures.vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabledFeatures.vk12.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.vk12);

    enabledFeatures.vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    enabledFeatures.vk13.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.vk13);

    if (devExtensions.extAttachmentFeedbackLoopLayout) {
      enabledFeatures.extAttachmentFeedbackLoopLayout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT;
      enabledFeatures.extAttachmentFeedbackLoopLayout.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extAttachmentFeedbackLoopLayout);
    }

    if (devExtensions.extCustomBorderColor) {
      enabledFeatures.extCustomBorderColor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
      enabledFeatures.extCustomBorderColor.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extCustomBorderColor);
    }

    if (devExtensions.extDepthClipEnable) {
      enabledFeatures.extDepthClipEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
      enabledFeatures.extDepthClipEnable.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extDepthClipEnable);
    }

    if (devExtensions.extGraphicsPipelineLibrary) {
      enabledFeatures.extGraphicsPipelineLibrary.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
      enabledFeatures.extGraphicsPipelineLibrary.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extGraphicsPipelineLibrary);
    }

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

    if (devExtensions.extTransformFeedback) {
      enabledFeatures.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
      enabledFeatures.extTransformFeedback.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extTransformFeedback);
    }

    if (devExtensions.extVertexAttributeDivisor.revision() >= 3) {
      enabledFeatures.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
      enabledFeatures.extVertexAttributeDivisor.pNext = std::exchange(enabledFeatures.core.pNext, &enabledFeatures.extVertexAttributeDivisor);
    }

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
    
    return new DxvkDevice(instance, this,
      new vk::DeviceFn(true, m_vki->instance(), device),
      devExtensions, enabledFeatures);
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
          VkDriverIdKHR       driver,
          uint32_t            minVer,
          uint32_t            maxVer) const {
    bool driverMatches = driver == m_deviceInfo.vk12.driverID;

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
  
  
  bool DxvkAdapter::isUnifiedMemoryArchitecture() const {
    auto memory = this->memoryProperties();
    bool result = true;

    for (uint32_t i = 0; i < memory.memoryHeapCount; i++)
      result &= memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

    return result;
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

    if (m_deviceExtensions.supports(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)) {
      m_deviceInfo.extGraphicsPipelineLibrary.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
      m_deviceInfo.extGraphicsPipelineLibrary.pNext = std::exchange(m_deviceInfo.core.pNext, &m_deviceInfo.extGraphicsPipelineLibrary);
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

    if (m_deviceExtensions.supports(VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME)) {
      m_deviceFeatures.extAttachmentFeedbackLoopLayout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT;
      m_deviceFeatures.extAttachmentFeedbackLoopLayout.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extAttachmentFeedbackLoopLayout);
    }

    if (m_deviceExtensions.supports(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME)) {
      m_deviceFeatures.extCustomBorderColor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT;
      m_deviceFeatures.extCustomBorderColor.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extCustomBorderColor);
    }

    if (m_deviceExtensions.supports(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME)) {
      m_deviceFeatures.extDepthClipEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
      m_deviceFeatures.extDepthClipEnable.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extDepthClipEnable);
    }

    if (m_deviceExtensions.supports(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)) {
      m_deviceFeatures.extGraphicsPipelineLibrary.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
      m_deviceFeatures.extGraphicsPipelineLibrary.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extGraphicsPipelineLibrary);
    }

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

    if (m_deviceExtensions.supports(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME)) {
      m_deviceFeatures.extTransformFeedback.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
      m_deviceFeatures.extTransformFeedback.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extTransformFeedback);
    }

    if (m_deviceExtensions.supports(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME) >= 3) {
      m_deviceFeatures.extVertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;
      m_deviceFeatures.extVertexAttributeDivisor.pNext = std::exchange(m_deviceFeatures.core.pNext, &m_deviceFeatures.extVertexAttributeDivisor);
    }

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
      "\n  multiViewport                          : ", features.core.features.multiViewport ? "1" : "0",
      "\n  samplerAnisotropy                      : ", features.core.features.samplerAnisotropy ? "1" : "0",
      "\n  textureCompressionBC                   : ", features.core.features.textureCompressionBC ? "1" : "0",
      "\n  occlusionQueryPrecise                  : ", features.core.features.occlusionQueryPrecise ? "1" : "0",
      "\n  pipelineStatisticsQuery                : ", features.core.features.pipelineStatisticsQuery ? "1" : "0",
      "\n  vertexPipelineStoresAndAtomics         : ", features.core.features.vertexPipelineStoresAndAtomics ? "1" : "0",
      "\n  fragmentStoresAndAtomics               : ", features.core.features.fragmentStoresAndAtomics ? "1" : "0",
      "\n  shaderImageGatherExtended              : ", features.core.features.shaderImageGatherExtended ? "1" : "0",
      "\n  shaderStorageImageExtendedFormats      : ", features.core.features.shaderStorageImageExtendedFormats ? "1" : "0",
      "\n  shaderStorageImageReadWithoutFormat    : ", features.core.features.shaderStorageImageReadWithoutFormat ? "1" : "0",
      "\n  shaderStorageImageWriteWithoutFormat   : ", features.core.features.shaderStorageImageWriteWithoutFormat ? "1" : "0",
      "\n  shaderClipDistance                     : ", features.core.features.shaderClipDistance ? "1" : "0",
      "\n  shaderCullDistance                     : ", features.core.features.shaderCullDistance ? "1" : "0",
      "\n  shaderFloat64                          : ", features.core.features.shaderFloat64 ? "1" : "0",
      "\n  shaderInt64                            : ", features.core.features.shaderInt64 ? "1" : "0",
      "\n  variableMultisampleRate                : ", features.core.features.variableMultisampleRate ? "1" : "0",
      "\nVulkan 1.1",
      "\n  shaderDrawParameters                   : ", features.vk11.shaderDrawParameters,
      "\nVulkan 1.2",
      "\n  samplerMirrorClampToEdge               : ", features.vk12.samplerMirrorClampToEdge,
      "\n  drawIndirectCount                      : ", features.vk12.drawIndirectCount,
      "\n  hostQueryReset                         : ", features.vk12.hostQueryReset,
      "\n  timelineSemaphore                      : ", features.vk12.timelineSemaphore,
      "\n  bufferDeviceAddress                    : ", features.vk12.bufferDeviceAddress,
      "\n  shaderOutputViewportIndex              : ", features.vk12.shaderOutputViewportIndex,
      "\n  shaderOutputLayer                      : ", features.vk12.shaderOutputLayer,
      "\n  timelineSemaphore                      : ", features.vk12.timelineSemaphore,
      "\nVulkan 1.3",
      "\n  robustImageAccess                      : ", features.vk13.robustImageAccess,
      "\n  pipelineCreationCacheControl           : ", features.vk13.pipelineCreationCacheControl,
      "\n  shaderDemoteToHelperInvocation         : ", features.vk13.shaderDemoteToHelperInvocation,
      "\n  shaderZeroInitializeWorkgroupMemory    : ", features.vk13.shaderZeroInitializeWorkgroupMemory,
      "\n  synchronization2                       : ", features.vk13.synchronization2,
      "\n  dynamicRendering                       : ", features.vk13.dynamicRendering,
      "\n", VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME,
      "\n  attachmentFeedbackLoopLayout           : ", features.extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout ? "1" : "0",
      "\n", VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME,
      "\n  customBorderColors                     : ", features.extCustomBorderColor.customBorderColors ? "1" : "0",
      "\n  customBorderColorWithoutFormat         : ", features.extCustomBorderColor.customBorderColorWithoutFormat ? "1" : "0",
      "\n", VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
      "\n  depthClipEnable                        : ", features.extDepthClipEnable.depthClipEnable ? "1" : "0",
      "\n", VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME,
      "\n  graphicsPipelineLibrary                : ", features.extGraphicsPipelineLibrary.graphicsPipelineLibrary ? "1" : "0",
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
      "\n", VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME,
      "\n  transformFeedback                      : ", features.extTransformFeedback.transformFeedback ? "1" : "0",
      "\n  geometryStreams                        : ", features.extTransformFeedback.geometryStreams ? "1" : "0",
      "\n", VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,
      "\n  vertexAttributeInstanceRateDivisor     : ", features.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor ? "1" : "0",
      "\n  vertexAttributeInstanceRateZeroDivisor : ", features.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor ? "1" : "0"));
  }


  void DxvkAdapter::logQueueFamilies(const DxvkAdapterQueueIndices& queues) {
    Logger::info(str::format("Queue families:",
      "\n  Graphics : ", queues.graphics,
      "\n  Transfer : ", queues.transfer));
  }
  
}
