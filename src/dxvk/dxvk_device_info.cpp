#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>
#include <type_traits>

#include "dxvk_device_info.h"
#include "dxvk_instance.h"
#include "dxvk_limits.h"

namespace dxvk {

  #define CORE_VERSIONS                            \
    HANDLE_CORE(vk11);                             \
    HANDLE_CORE(vk12);                             \
    HANDLE_CORE(vk13);                             \

  #define EXTENSIONS_WITH_FEATURES                 \
    HANDLE_EXT(extAttachmentFeedbackLoopLayout);   \
    HANDLE_EXT(extBorderColorSwizzle);             \
    HANDLE_EXT(extConservativeRasterization);      \
    HANDLE_EXT(extCustomBorderColor);              \
    HANDLE_EXT(extDepthClipEnable);                \
    HANDLE_EXT(extDepthBiasControl);               \
    HANDLE_EXT(extDescriptorBuffer);               \
    HANDLE_EXT(extDescriptorHeap);                 \
    HANDLE_EXT(extExtendedDynamicState3);          \
    HANDLE_EXT(extFragmentShaderInterlock);        \
    HANDLE_EXT(extFullScreenExclusive);            \
    HANDLE_EXT(extGraphicsPipelineLibrary);        \
    HANDLE_EXT(extHdrMetadata);                    \
    HANDLE_EXT(extLineRasterization);              \
    HANDLE_EXT(extMemoryBudget);                   \
    HANDLE_EXT(extMemoryPriority);                 \
    HANDLE_EXT(extMultiDraw);                      \
    HANDLE_EXT(extNonSeamlessCubeMap);             \
    HANDLE_EXT(extPageableDeviceLocalMemory);      \
    HANDLE_EXT(extRobustness2);                    \
    HANDLE_EXT(extSampleLocations);                \
    HANDLE_EXT(extShaderModuleIdentifier);         \
    HANDLE_EXT(extShaderStencilExport);            \
    HANDLE_EXT(extSwapchainColorSpace);            \
    HANDLE_EXT(extSwapchainMaintenance1);          \
    HANDLE_EXT(extTransformFeedback);              \
    HANDLE_EXT(extVertexAttributeDivisor);         \
    HANDLE_EXT(khrExternalMemoryWin32);            \
    HANDLE_EXT(khrExternalSemaphoreWin32);         \
    HANDLE_EXT(khrLoadStoreOpNone);                \
    HANDLE_EXT(khrMaintenance5);                   \
    HANDLE_EXT(khrMaintenance6);                   \
    HANDLE_EXT(khrMaintenance7);                   \
    HANDLE_EXT(khrMaintenance8);                   \
    HANDLE_EXT(khrMaintenance9);                   \
    HANDLE_EXT(khrMaintenance10);                  \
    HANDLE_EXT(khrPipelineLibrary);                \
    HANDLE_EXT(khrPresentId);                      \
    HANDLE_EXT(khrPresentId2);                     \
    HANDLE_EXT(khrPresentWait);                    \
    HANDLE_EXT(khrPresentWait2);                   \
    HANDLE_EXT(khrShaderFloatControls2);           \
    HANDLE_EXT(khrShaderSubgroupUniformControlFlow);\
    HANDLE_EXT(khrShaderUntypedPointers);          \
    HANDLE_EXT(khrSwapchain);                      \
    HANDLE_EXT(khrSwapchainMaintenance1);          \
    HANDLE_EXT(khrSwapchainMutableFormat);         \
    HANDLE_EXT(khrUnifiedImageLayouts);            \
    HANDLE_EXT(khrWin32KeyedMutex);                \
    HANDLE_EXT(nvLowLatency2);                     \
    HANDLE_EXT(nvRawAccessChains);                 \
    HANDLE_EXT(nvxBinaryImport);                   \
    HANDLE_EXT(nvxImageViewHandle);

  #define EXTENSIONS_WITH_PROPERTIES               \
    HANDLE_EXT(extConservativeRasterization);      \
    HANDLE_EXT(extCustomBorderColor);              \
    HANDLE_EXT(extDescriptorBuffer);               \
    HANDLE_EXT(extDescriptorHeap);                 \
    HANDLE_EXT(extExtendedDynamicState3);          \
    HANDLE_EXT(extGraphicsPipelineLibrary);        \
    HANDLE_EXT(extLineRasterization);              \
    HANDLE_EXT(extMultiDraw);                      \
    HANDLE_EXT(extRobustness2);                    \
    HANDLE_EXT(extSampleLocations);                \
    HANDLE_EXT(extTransformFeedback);              \
    HANDLE_EXT(extVertexAttributeDivisor);         \
    HANDLE_EXT(khrMaintenance5);                   \
    HANDLE_EXT(khrMaintenance6);                   \
    HANDLE_EXT(khrMaintenance7);                   \
    HANDLE_EXT(khrMaintenance9);                   \
    HANDLE_EXT(khrMaintenance10);


  DxvkDeviceCapabilities::DxvkDeviceCapabilities(
    const DxvkInstance&               instance,
          VkPhysicalDevice            adapter,
    const VkDeviceCreateInfo*         deviceInfo) {
    // Can't query anything on a Vulkan 1.0 device
    auto vk = instance.vki();
    vk->vkGetPhysicalDeviceProperties(adapter, &m_properties.core.properties);

    if (m_properties.core.properties.apiVersion < DxvkVulkanApiVersion)
      return;

    initSupportedExtensions(instance, adapter, deviceInfo);
    initSupportedFeatures(instance, adapter, deviceInfo);
    initDeviceProperties(instance, adapter, deviceInfo);
    initQueueProperties(instance, adapter, deviceInfo);
    initMemoryProperties(instance, adapter);

    disableUnusedFeatures(instance);

    enableFeaturesAndExtensions();
    enableQueues();
  }


  DxvkDeviceCapabilities::~DxvkDeviceCapabilities() {

  }


  bool DxvkDeviceCapabilities::queryDeviceExtensions(
          uint32_t*                   count,
          VkExtensionProperties*      extensions) const {
    if (!extensions) {
      *count = m_extensionList.size();
      return true;
    }

    if (*count > m_extensionList.size())
      *count = m_extensionList.size();

    for (uint32_t i = 0u; i < *count; i++)
      extensions[i] = *(m_extensionList[i]);

    return *count >= m_extensionList.size();
  }


  bool DxvkDeviceCapabilities::queryDeviceQueues(
          uint32_t*                   count,
          VkDeviceQueueCreateInfo*    queues) const {
    if (!queues) {
      *count = m_queuesEnabled.size();
      return true;
    }

    if (*count > m_queuesEnabled.size())
      *count = m_queuesEnabled.size();

    bool complete = *count >= m_queuesEnabled.size();

    for (uint32_t i = 0u; i < *count; i++) {
      const auto& in = m_queuesEnabled[i];

      if (queues[i].pQueuePriorities) {
        complete = complete && queues[i].queueCount >= in.queueCount;

        for (uint32_t j = 0u; j < in.queueCount && j < queues[i].queueCount; j++)
          const_cast<float&>(queues[i].pQueuePriorities[j]) = in.pQueuePriorities[j];
      }

      queues[i].flags = in.flags;
      queues[i].queueFamilyIndex = in.queueFamilyIndex;
      queues[i].queueCount = in.queueCount;
    }

    return complete;
  }


  bool DxvkDeviceCapabilities::queryDeviceFeatures(
          size_t*                     size,
          void*                       data) const {
    if (!data) {
      *size = sizeof(m_featuresEnabled);
      return true;
    }

    if (*size > sizeof(m_featuresEnabled))
      *size = sizeof(m_featuresEnabled);

    std::memcpy(data, &m_featuresEnabled, *size);
    return *size >= sizeof(m_featuresEnabled);
  }


  bool DxvkDeviceCapabilities::isSuitable(size_t errorSize, char* error) {
    auto message = checkDeviceCompatibility();

    if (message && errorSize)
      std::strncpy(error, message->c_str(), errorSize - 1u);

    return !message;
  }


  void DxvkDeviceCapabilities::logDeviceInfo() {
    // Assume that known features are ordered by extension
    const VkExtensionProperties* extension = nullptr;

    std::stringstream stream;
    stream << m_properties.core.properties.deviceName << ":" << std::endl
           << "  Driver   : " << m_properties.vk12.driverName << " " << m_properties.driverVersion.toString() << std::endl;

    stream << "Queues:" << std::endl
           << "  Graphics : (" << m_queueMapping.graphics.family << ", " << m_queueMapping.graphics.index << ")" << std::endl
           << "  Transfer : (" << m_queueMapping.transfer.family << ", " << m_queueMapping.transfer.index << ")" << std::endl
           << "  Sparse   : (" << m_queueMapping.sparse.family   << ", " << m_queueMapping.sparse.index   << ")" << std::endl;

    // Log memory type and heap properties
    static const std::array<std::pair<VkMemoryPropertyFlagBits, const char*>, 8> s_flags = {{
      { VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,        "DEVICE_LOCAL"        },
      { VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,        "HOST_VISIBLE"        },
      { VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,       "HOST_COHERENT"       },
      { VK_MEMORY_PROPERTY_HOST_CACHED_BIT,         "HOST_CACHED"         },
      { VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,    "LAZILY_ALLOCATED"    },
      { VK_MEMORY_PROPERTY_PROTECTED_BIT,           "PROTECTED"           },
      { VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD, "DEVICE_COHERENT"     },
      { VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD, "DEVICE_UNCACHED"     },
    }};

    stream << "Memory:" << std::endl;

    for (uint32_t h = 0u; h < m_memory.core.memoryProperties.memoryHeapCount; h++) {
      const auto& heap = m_memory.core.memoryProperties.memoryHeaps[h];
      stream << "  Heap " << h << ": ";

      if (heap.size >= (1ull << 30u)) {
        auto size = (heap.size * 100u) >> 30u;
        stream << (size / 100u) << "." << (size % 100u) << " GiB";
      } else {
        stream << (heap.size >> 20u) << " MiB";
      }

      if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        stream << " (DEVICE_LOCAL)";

      stream << std::endl;

      if (m_featuresSupported.extMemoryBudget) {
        stream << "  Budget: ";

        if (m_memory.budget.heapBudget[h] >= (1ull << 30u)) {
          auto budget = (m_memory.budget.heapBudget[h] * 100u) >> 30u;
          stream << (budget / 100u) << "." << (budget % 100u) << " GiB";
        } else  {
          stream << (m_memory.budget.heapBudget[h] >> 20u) << " MiB";
        }

        stream << std::endl;
      }

      for (uint32_t t = 0u; t < m_memory.core.memoryProperties.memoryTypeCount; t++) {
        const auto& type = m_memory.core.memoryProperties.memoryTypes[t];

        if (type.heapIndex != h)
          continue;

        stream << "    Type " << std::setw(2u) << t << ": ";

        const char* prefix = "";

        for (const auto& f : s_flags) {
          if (!(type.propertyFlags & f.first))
            continue;

          stream << prefix << f.second;
          prefix = " | ";
        }

        if (!type.propertyFlags)
          stream << "(None)";

        stream << std::endl;
      }
    }


    stream << "Enabled extensions:" << std::endl;

    for (const auto& e : m_extensionList)
      stream << "  " << e->extensionName << std::endl;

    stream << "Enabled features:" << std::endl;

    for (const auto& f : getFeatureList()) {
      if (extension != f.extensionEnabled) {
        extension = f.extensionEnabled;

        if (extension)
          stream << extension->extensionName << ":" << std::endl;
      }

      stream << "  " << std::setfill(' ') << std::left << std::setw(30) << f.readableName << " : " << uint32_t(*f.featureEnabled) << std::endl;
    }

    Logger::info(stream.str());
  }


  void DxvkDeviceCapabilities::initSupportedExtensions(
    const DxvkInstance&               instance,
          VkPhysicalDevice            adapter,
    const VkDeviceCreateInfo*         deviceInfo) {
    auto vk = instance.vki();

    uint32_t extensionCount = 0u;
    vk->vkEnumerateDeviceExtensionProperties(adapter, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vk->vkEnumerateDeviceExtensionProperties(adapter, nullptr, &extensionCount, extensions.data());

    // Order extensions by name to accelerate lookup
    std::sort(extensions.begin(), extensions.end(), vk::SortExtension());

    // If we are importing a device with pre-defined extensions,
    // filter out any extensions that are not enabled
    if (deviceInfo) {
      std::set<VkExtensionProperties, vk::SortExtension> enabledExtensions = { };

      for (uint32_t i = 0u; i < deviceInfo->enabledExtensionCount; i++)
        enabledExtensions.insert(vk::makeExtension(deviceInfo->ppEnabledExtensionNames[i]));

      extensions.erase(std::remove_if(extensions.begin(), extensions.end(),
        [&enabledExtensions] (const VkExtensionProperties& a) {
          return enabledExtensions.find(a) == enabledExtensions.end();
        }), extensions.end());
    }

    // If multiple extensions provide the same functionality, remove any
    // deprecated aliases so that we always use the latest iteration.
    std::array<std::pair<const char*, const char*>, 1u> aliases = {{
      { VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME },
    }};

    for (const auto& alias : aliases) {
      auto a = vk::makeExtension(alias.first);
      auto b = vk::makeExtension(alias.second);

      auto aIter = std::lower_bound(extensions.begin(), extensions.end(), a, vk::SortExtension());
      auto bIter = std::lower_bound(extensions.begin(), extensions.end(), b, vk::SortExtension());

      if (aIter != extensions.end() && !vk::SortExtension()(a, *aIter)
       && bIter != extensions.end() && !vk::SortExtension()(b, *bIter))
        extensions.erase(bIter);
    }

    // HACK: Use mesh shader extension support to determine whether we're
    // running on older (pre-Turing) Nvidia GPUs.
    m_hasMeshShader = std::find_if(extensions.begin(), extensions.end(),
      [] (const VkExtensionProperties& ext) {
        return !std::strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME);
      }) != extensions.end();

    // HACK: Use fmask extension to detect pre-RDNA3 hardware.
    m_hasFmask = std::find_if(extensions.begin(), extensions.end(),
      [] (const VkExtensionProperties& ext) {
        return !std::strcmp(ext.extensionName, VK_AMD_SHADER_FRAGMENT_MASK_EXTENSION_NAME);
      }) != extensions.end();

    // Use the supported spec version as a way to indicate extension support.
    // We may ignore certain extensions if the spec version is too old.
    for (const auto& f : getFeatureList()) {
      if (!f.extensionSupported)
        continue;

      auto iter = std::lower_bound(extensions.begin(), extensions.end(), *f.extensionSupported, vk::SortExtension());

      if (iter != extensions.end()) {
        if (!vk::SortExtension()(*f.extensionSupported, *iter))
          f.extensionSupported->specVersion = std::max(1u, iter->specVersion);
      }
    }
  }


  void DxvkDeviceCapabilities::initSupportedFeatures(
    const DxvkInstance&               instance,
          VkPhysicalDevice            adapter,
    const VkDeviceCreateInfo*         deviceInfo) {
    auto vk = instance.vki();

    chainFeatures(m_extensionsSupported, m_featuresSupported);

    if (deviceInfo) {
      // Only consider features enabled on the device as supported
      if (deviceInfo->pEnabledFeatures)
        m_featuresSupported.core.features = *deviceInfo->pEnabledFeatures;

      copyFeature(deviceInfo->pNext, nullptr, &m_featuresSupported.core);

      #define HANDLE_CORE(name) copyFeature(deviceInfo->pNext, nullptr, &m_featuresSupported.name)
      #define HANDLE_EXT(name) copyFeature(deviceInfo->pNext, &m_extensionsSupported.name, &m_featuresSupported.name)

      CORE_VERSIONS
      EXTENSIONS_WITH_FEATURES

      #undef HANDLE_CORE
      #undef HANDLE_EXT
    } else {
      // Query supported features from the physical device
      vk->vkGetPhysicalDeviceFeatures2(adapter, &m_featuresSupported.core);
    }
  }


  void DxvkDeviceCapabilities::initDeviceProperties(
    const DxvkInstance&               instance,
          VkPhysicalDevice            adapter,
    const VkDeviceCreateInfo*         deviceInfo) {
    auto vk = instance.vki();

    chainProperties(m_extensionsSupported, m_properties);
    vk->vkGetPhysicalDeviceProperties2(adapter, &m_properties.core);

    m_properties.driverVersion = decodeDriverVersion(
      m_properties.vk12.driverID, m_properties.core.properties.driverVersion);
  }


  void DxvkDeviceCapabilities::initQueueProperties(
    const DxvkInstance&               instance,
          VkPhysicalDevice            adapter,
    const VkDeviceCreateInfo*         deviceInfo) {
    auto vk = instance.vki();

    uint32_t queueCount = 0u;
    vk->vkGetPhysicalDeviceQueueFamilyProperties2(adapter, &queueCount, nullptr);

    // Use local array of base structures as the API requires,
    // then copy the base structure back to the metadata array
    std::vector<VkQueueFamilyProperties2> queueFamilies(queueCount, { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });

    // Chain extension structs directly into the metadata structure
    m_queuesAvailable.resize(queueCount);

    for (uint32_t i = 0u; i < queueCount; i++) {
      auto& base = queueFamilies[i];
      auto& meta = m_queuesAvailable[i];

      if (m_featuresSupported.khrMaintenance9.maintenance9)
        meta.ownershipTransfer.pNext = std::exchange(base.pNext, &meta.ownershipTransfer);
    }

    vk->vkGetPhysicalDeviceQueueFamilyProperties2(adapter, &queueCount, queueFamilies.data());

    for (uint32_t i = 0u; i < queueCount; i++)
      m_queuesAvailable[i].core = queueFamilies[i];

    if (deviceInfo) {
      // Only mark queues available that the device has been created with
      for (uint32_t i = 0u; i < queueCount; i++) {
        uint32_t queueCount = 0u;

        for (uint32_t j = 0u; j < deviceInfo->queueCreateInfoCount && !queueCount; j++) {
          if (deviceInfo->pQueueCreateInfos[j].queueFamilyIndex == i)
            queueCount = deviceInfo->pQueueCreateInfos[j].queueCount;
        }

        m_queuesAvailable[i].core.queueFamilyProperties.queueCount = queueCount;
      }
    }
  }


  void DxvkDeviceCapabilities::initMemoryProperties(
    const DxvkInstance&               instance,
          VkPhysicalDevice            adapter) {
    auto vk = instance.vki();

    if (m_featuresSupported.extMemoryBudget)
      m_memory.core.pNext = &m_memory.budget;

    vk->vkGetPhysicalDeviceMemoryProperties2(adapter, &m_memory.core);
  }


  void DxvkDeviceCapabilities::disableUnusedFeatures(
    const DxvkInstance&               instance) {
    if (m_featuresSupported.extDescriptorHeap.descriptorHeap) {
      // Only enable descriptor heaps on drivers that are either known to work,
      // or are maintained well enough that any issues are likely to get fixed
      bool enableDescriptorHeap = m_properties.vk12.driverID == VK_DRIVER_ID_MESA_RADV
                               || m_properties.vk12.driverID == VK_DRIVER_ID_MESA_NVK
                               || m_properties.vk12.driverID == VK_DRIVER_ID_MESA_LLVMPIPE
                               || m_properties.vk12.driverID == VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA
                               || m_properties.vk12.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY;

      applyTristate(enableDescriptorHeap, instance.options().enableDescriptorHeap);

      if (!enableDescriptorHeap)
        m_featuresSupported.extDescriptorHeap.descriptorHeap = VK_FALSE;
    }

    // Descriptor heap deprecates descriptor buffer
    if (m_featuresSupported.extDescriptorHeap.descriptorHeap)
      m_featuresSupported.extDescriptorBuffer.descriptorBuffer = VK_FALSE;

    // Descriptor buffers cause perf regressions on some GPUs
    if (m_featuresSupported.extDescriptorBuffer.descriptorBuffer) {
      bool enableDescriptorBuffer = m_properties.vk12.driverID == VK_DRIVER_ID_MESA_RADV
                                 || m_properties.vk12.driverID == VK_DRIVER_ID_MESA_NVK
                                 || m_properties.vk12.driverID == VK_DRIVER_ID_MESA_LLVMPIPE;

      // Pascal reportedly sees massive perf drops with descriptor buffer
      if (m_properties.vk12.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY)
        enableDescriptorBuffer = m_hasMeshShader;

      // On RDNA2 and older, descriptor buffer implicitly disables fmask
      // on amdvlk, which makes MSAA performance unusable on these GPUs.
      if (m_properties.vk12.driverID == VK_DRIVER_ID_AMD_OPEN_SOURCE
       || m_properties.vk12.driverID == VK_DRIVER_ID_AMD_PROPRIETARY)
        enableDescriptorBuffer = !m_hasFmask;

      applyTristate(enableDescriptorBuffer, instance.options().enableDescriptorBuffer);

      if (!enableDescriptorBuffer)
        m_featuresSupported.extDescriptorBuffer.descriptorBuffer = VK_FALSE;
    }

    // Disable unified layouts if disabled via config
    if (!instance.options().enableUnifiedImageLayout)
      m_featuresSupported.khrUnifiedImageLayouts.unifiedImageLayouts = VK_FALSE;

    if (env::is32BitHostPlatform()) {
      // CUDA interop is unnecessary on 32-bit, no games use it
      m_featuresSupported.nvxBinaryImport = VK_FALSE;
      m_featuresSupported.nvxImageViewHandle = VK_FALSE;

      // Reflex is broken on 32-bit
      m_featuresSupported.nvLowLatency2 = VK_FALSE;
    }

    // EXT_multi_draw is broken on proprietary qcom on some devices
    if (m_properties.vk12.driverID == VK_DRIVER_ID_QUALCOMM_PROPRIETARY)
      m_featuresSupported.extMultiDraw.multiDraw = VK_FALSE;

    // If we're running off a device without a sparse binding queue,
    // disable all the sparse binding features as well
    uint32_t sparseQueue = findQueueFamily(VK_QUEUE_SPARSE_BINDING_BIT, VK_QUEUE_SPARSE_BINDING_BIT);

    if (sparseQueue == VK_QUEUE_FAMILY_IGNORED
     || !m_featuresSupported.core.features.sparseBinding
     || !m_featuresSupported.core.features.sparseResidencyBuffer
     || !m_featuresSupported.core.features.sparseResidencyImage2D
     || !m_featuresSupported.core.features.sparseResidencyAliased) {
      m_featuresSupported.core.features.sparseBinding = VK_FALSE;
      m_featuresSupported.core.features.sparseResidencyBuffer = VK_FALSE;
      m_featuresSupported.core.features.sparseResidencyImage2D = VK_FALSE;
      m_featuresSupported.core.features.sparseResidencyImage3D = VK_FALSE;
      m_featuresSupported.core.features.sparseResidency2Samples = VK_FALSE;
      m_featuresSupported.core.features.sparseResidency4Samples = VK_FALSE;
      m_featuresSupported.core.features.sparseResidency8Samples = VK_FALSE;
      m_featuresSupported.core.features.sparseResidency16Samples = VK_FALSE;
      m_featuresSupported.core.features.sparseResidencyAliased = VK_FALSE;
    }

    // robustness2 is stronger than the Vulkan 1.3 feature
    if (m_featuresSupported.extRobustness2.robustImageAccess2)
      m_featuresSupported.vk13.robustImageAccess = VK_FALSE;

    // Vertex attribute divisor is unusable before spec version 3
    if (m_extensionsSupported.extVertexAttributeDivisor.specVersion < 3u) {
      m_featuresSupported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor = VK_FALSE;
      m_featuresSupported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor = VK_FALSE;
    }

    // For line rasterization, ensure that the feature set actually makes sense
    if (!m_featuresSupported.core.features.wideLines || !m_featuresSupported.extLineRasterization.rectangularLines) {
      m_featuresSupported.core.features.wideLines = VK_FALSE;
      m_featuresSupported.extLineRasterization.rectangularLines = VK_FALSE;
      m_featuresSupported.extLineRasterization.smoothLines = VK_FALSE;
    }

    // Ensure we only enable one of present_id or present_id_2. Prefer the
    // older versions of the present_id/wait extensions since the newer ones
    // cause issues with external layers and apparently some Wayland setups
    // on Mesa for unknown reasons.
    if (m_featuresSupported.khrPresentId.presentId)
      m_featuresSupported.khrPresentId2.presentId2 = VK_FALSE;

    // Sanitize features with other feature dependencies
    if (!m_featuresSupported.core.features.shaderInt16)
      m_featuresSupported.vk11.storagePushConstant16 = VK_FALSE;

    if (!m_featuresSupported.khrPresentId2.presentId2)
      m_featuresSupported.khrPresentWait2.presentWait2 = VK_FALSE;

    if (!m_featuresSupported.khrPresentId.presentId)
      m_featuresSupported.khrPresentWait.presentWait = VK_FALSE;

    if (!m_featuresSupported.khrPresentId.presentId
     && !m_featuresSupported.khrPresentId2.presentId2)
      m_featuresSupported.nvLowLatency2 = VK_FALSE;
  }


  void DxvkDeviceCapabilities::enableFeaturesAndExtensions() {
    // Some extensions functionally work as "physical device" extensions
    // and will not be explicitly enabled during device creation.
    static const std::array<VkExtensionProperties, 1u> s_passiveExtensions = {{
      vk::makeExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME),
    }};

    for (const auto& f : getFeatureList()) {
      // Enable any supported feature that we know about
      if ((*f.featureEnabled = *f.featureSupported)) {
        // Also enable the corresponding extension if we haven't done so yet'
        if (f.extensionEnabled && !f.extensionEnabled->specVersion) {
          f.extensionEnabled->specVersion = f.extensionSupported->specVersion;

          auto entry = std::find_if(s_passiveExtensions.begin(), s_passiveExtensions.end(),
            [&f] (const VkExtensionProperties& passive) {
              return !std::strncmp(f.extensionEnabled->extensionName, passive.extensionName, sizeof(passive.extensionName));
            });

          if (entry == s_passiveExtensions.end())
            m_extensionList.push_back(f.extensionEnabled);
        }
      }
    }

    // Make sure we have a full pNext chain to pass to the device
    chainFeatures(m_extensionsEnabled, m_featuresEnabled);
  }


  void DxvkDeviceCapabilities::enableQueues() {
    m_queueMapping.graphics.family = findQueueFamily(
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    if (m_queueMapping.graphics.family == VK_QUEUE_FAMILY_IGNORED)
      return;

    uint32_t computeQueue = findQueueFamily(
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
      VK_QUEUE_COMPUTE_BIT);

    if (computeQueue == VK_QUEUE_FAMILY_IGNORED)
      computeQueue = m_queueMapping.graphics.family;

    m_queueMapping.transfer.family = findQueueFamily(
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
      VK_QUEUE_TRANSFER_BIT);

    if (m_queueMapping.transfer.family == VK_QUEUE_FAMILY_IGNORED)
      m_queueMapping.transfer.family = computeQueue;

    // Prefer using the graphics queue as a sparse binding queue if possible
    auto& graphicsQueue = m_queuesAvailable[m_queueMapping.graphics.family].core;

    if (graphicsQueue.queueFamilyProperties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
      m_queueMapping.sparse.family = m_queueMapping.graphics.family;
    } else {
      m_queueMapping.sparse.family = findQueueFamily(
        VK_QUEUE_SPARSE_BINDING_BIT,
        VK_QUEUE_SPARSE_BINDING_BIT);
    }

    // Actually enable all the queues
    enableQueue(m_queueMapping.graphics);
    enableQueue(m_queueMapping.transfer);
    enableQueue(m_queueMapping.sparse);

    // Fix up queue priority pointers
    uint32_t maxQueueCount = 0u;

    for (auto& q : m_queuesEnabled)
      maxQueueCount = std::max(maxQueueCount, q.queueCount);

    m_queuePriorities.resize(maxQueueCount, 1.0f);

    for (auto& q : m_queuesEnabled)
      q.pQueuePriorities = m_queuePriorities.data();
  }


  void DxvkDeviceCapabilities::enableQueue(
          DxvkDeviceQueueIndex        queue) {
    if (queue.family == VK_QUEUE_FAMILY_IGNORED)
      return;

    for (auto& q : m_queuesEnabled) {
      if (q.queueFamilyIndex == queue.family) {
        q.queueCount = queue.index + 1u;
        return;
      }
    }

    auto& q = m_queuesEnabled.emplace_back();
    q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q.queueFamilyIndex = queue.family;
    q.queueCount = queue.index + 1u;
  }


  uint32_t DxvkDeviceCapabilities::findQueueFamily(
          VkQueueFlags                mask,
          VkQueueFlags                flags) const {
    for (uint32_t i = 0; i < m_queuesAvailable.size(); i++) {
      if ((m_queuesAvailable[i].core.queueFamilyProperties.queueFlags & mask) == flags
       && (m_queuesAvailable[i].core.queueFamilyProperties.queueCount))
        return i;
    }

    return VK_QUEUE_FAMILY_IGNORED;
  }


  std::optional<std::string> DxvkDeviceCapabilities::checkDeviceCompatibility() {
    if (m_properties.core.properties.apiVersion < DxvkVulkanApiVersion) {
      return str::format("Device does not support Vulkan ",
        VK_API_VERSION_MAJOR(DxvkVulkanApiVersion), ".",
        VK_API_VERSION_MINOR(DxvkVulkanApiVersion));
    }

    if (m_queueMapping.graphics.family == VK_QUEUE_FAMILY_IGNORED)
      return std::string("Device does not have a graphics queue");

    for (const auto& f : getFeatureList()) {
      if (f.featureRequired && !(*f.featureEnabled)) {
        std::string message = str::format("Device does not support required feature '", f.readableName, "'");

        if (f.extensionEnabled)
          message += str::format(" (extension: ", f.extensionEnabled->extensionName, ")");

        return message;
      }
    }

    if (!m_featuresEnabled.extDescriptorHeap.descriptorHeap && m_properties.core.properties.limits.maxPushConstantsSize < MaxTotalPushDataSize)
      return str::format("Device does not support ", MaxTotalPushDataSize, " of push data");

    return std::nullopt;
  }


  void DxvkDeviceCapabilities::chainFeatures(
    const DxvkDeviceExtensionInfo&    extensions,
          DxvkDeviceFeatures&         features) {
    #define HANDLE_CORE(name) chainFeature(nullptr, &features.core, &features.name)
    #define HANDLE_EXT(name) chainFeature(&extensions.name, &features.core, &features.name)

    CORE_VERSIONS
    EXTENSIONS_WITH_FEATURES

    #undef HANDLE_CORE
    #undef HANDLE_EXT
  }


  void DxvkDeviceCapabilities::chainProperties(
    const DxvkDeviceExtensionInfo&    extensions,
          DxvkDeviceInfo&             properties) {
    #define HANDLE_CORE(name) chainProperties(nullptr, &properties.core, &properties.name)
    #define HANDLE_EXT(name) chainProperties(&extensions.name, &properties.core, &properties.name)

    CORE_VERSIONS
    EXTENSIONS_WITH_PROPERTIES

    #undef HANDLE_CORE
    #undef HANDLE_EXT
  }


  std::vector<DxvkDeviceCapabilities::FeatureEntry> DxvkDeviceCapabilities::getFeatureList() {
    #define ENABLE_FEATURE(version, name, require)        \
      FeatureEntry { nullptr, nullptr,                    \
        &m_featuresSupported.version.name,                \
        &m_featuresEnabled.version.name,                  \
        require, #name }

    #define ENABLE_EXT(ext, require)                      \
      FeatureEntry {                                      \
        &m_extensionsSupported.ext,                       \
        &m_extensionsEnabled.ext,                         \
        &m_featuresSupported.ext,                         \
        &m_featuresEnabled.ext,                           \
        require, #ext }

    #define ENABLE_EXT_FEATURE(ext, name, require)        \
      FeatureEntry {                                      \
        &m_extensionsSupported.ext,                       \
        &m_extensionsEnabled.ext,                         \
        &m_featuresSupported.ext.name,                    \
        &m_featuresEnabled.ext.name,                      \
        require, #name }

    return {{
      ENABLE_FEATURE(core.features, depthBiasClamp, true),
      ENABLE_FEATURE(core.features, depthBounds, false),
      ENABLE_FEATURE(core.features, depthClamp, true),
      ENABLE_FEATURE(core.features, drawIndirectFirstInstance, false),
      ENABLE_FEATURE(core.features, dualSrcBlend, true),
      ENABLE_FEATURE(core.features, fillModeNonSolid, true),
      ENABLE_FEATURE(core.features, fragmentStoresAndAtomics, false),
      ENABLE_FEATURE(core.features, fullDrawIndexUint32, true),
      ENABLE_FEATURE(core.features, geometryShader, true),
      ENABLE_FEATURE(core.features, imageCubeArray, true),
      ENABLE_FEATURE(core.features, independentBlend, true),
      ENABLE_FEATURE(core.features, logicOp, false),
      ENABLE_FEATURE(core.features, multiDrawIndirect, true),
      ENABLE_FEATURE(core.features, multiViewport, true),
      ENABLE_FEATURE(core.features, occlusionQueryPrecise, true),
      ENABLE_FEATURE(core.features, pipelineStatisticsQuery, false),
      ENABLE_FEATURE(core.features, robustBufferAccess, true),
      ENABLE_FEATURE(core.features, sampleRateShading, true),
      ENABLE_FEATURE(core.features, samplerAnisotropy, false),
      ENABLE_FEATURE(core.features, shaderClipDistance, true),
      ENABLE_FEATURE(core.features, shaderCullDistance, true),
      ENABLE_FEATURE(core.features, shaderFloat64, false),
      ENABLE_FEATURE(core.features, shaderImageGatherExtended, true),
      ENABLE_FEATURE(core.features, shaderInt16, false),
      ENABLE_FEATURE(core.features, shaderInt64, true),
      ENABLE_FEATURE(core.features, shaderUniformBufferArrayDynamicIndexing, false),
      ENABLE_FEATURE(core.features, shaderSampledImageArrayDynamicIndexing, true),
      ENABLE_FEATURE(core.features, shaderStorageBufferArrayDynamicIndexing, false),
      ENABLE_FEATURE(core.features, shaderStorageImageArrayDynamicIndexing, false),
      ENABLE_FEATURE(core.features, sparseBinding, false),
      ENABLE_FEATURE(core.features, sparseResidencyBuffer, false),
      ENABLE_FEATURE(core.features, sparseResidencyImage2D, false),
      ENABLE_FEATURE(core.features, sparseResidencyImage3D, false),
      ENABLE_FEATURE(core.features, sparseResidency2Samples, false),
      ENABLE_FEATURE(core.features, sparseResidency4Samples, false),
      ENABLE_FEATURE(core.features, sparseResidency8Samples, false),
      ENABLE_FEATURE(core.features, sparseResidency16Samples, false),
      ENABLE_FEATURE(core.features, sparseResidencyAliased, false),
      ENABLE_FEATURE(core.features, shaderResourceResidency, false),
      ENABLE_FEATURE(core.features, shaderResourceMinLod, false),
      ENABLE_FEATURE(core.features, tessellationShader, false),
      ENABLE_FEATURE(core.features, textureCompressionBC, true),
      ENABLE_FEATURE(core.features, variableMultisampleRate, false),
      ENABLE_FEATURE(core.features, vertexPipelineStoresAndAtomics, false),
      ENABLE_FEATURE(core.features, wideLines, false),

      ENABLE_FEATURE(vk11, shaderDrawParameters, true),
      ENABLE_FEATURE(vk11, storagePushConstant16, false),

      ENABLE_FEATURE(vk12, bufferDeviceAddress, true),
      ENABLE_FEATURE(vk12, descriptorIndexing, true),
      ENABLE_FEATURE(vk12, shaderUniformTexelBufferArrayDynamicIndexing, false),
      ENABLE_FEATURE(vk12, shaderStorageTexelBufferArrayDynamicIndexing, false),
      ENABLE_FEATURE(vk12, shaderUniformBufferArrayNonUniformIndexing, false),
      ENABLE_FEATURE(vk12, shaderSampledImageArrayNonUniformIndexing, false),
      ENABLE_FEATURE(vk12, shaderStorageBufferArrayNonUniformIndexing, false),
      ENABLE_FEATURE(vk12, shaderStorageImageArrayNonUniformIndexing, false),
      ENABLE_FEATURE(vk12, shaderUniformTexelBufferArrayNonUniformIndexing, false),
      ENABLE_FEATURE(vk12, shaderStorageTexelBufferArrayNonUniformIndexing, false),
      ENABLE_FEATURE(vk12, descriptorBindingSampledImageUpdateAfterBind, true),
      ENABLE_FEATURE(vk12, descriptorBindingUpdateUnusedWhilePending, true),
      ENABLE_FEATURE(vk12, descriptorBindingPartiallyBound, true),
      ENABLE_FEATURE(vk12, drawIndirectCount, false),
      ENABLE_FEATURE(vk12, hostQueryReset, true),
      ENABLE_FEATURE(vk12, runtimeDescriptorArray, true),
      ENABLE_FEATURE(vk12, samplerFilterMinmax, false),
      ENABLE_FEATURE(vk12, samplerMirrorClampToEdge, true),
      ENABLE_FEATURE(vk12, scalarBlockLayout, true),
      ENABLE_FEATURE(vk12, shaderFloat16, false),
      ENABLE_FEATURE(vk12, shaderInt8, false),
      ENABLE_FEATURE(vk12, shaderOutputViewportIndex, false),
      ENABLE_FEATURE(vk12, shaderOutputLayer, false),
      ENABLE_FEATURE(vk12, timelineSemaphore, true),
      ENABLE_FEATURE(vk12, uniformBufferStandardLayout, true),
      ENABLE_FEATURE(vk12, vulkanMemoryModel, true),

      ENABLE_FEATURE(vk13, computeFullSubgroups, true),
      ENABLE_FEATURE(vk13, dynamicRendering, true),
      ENABLE_FEATURE(vk13, maintenance4, true),
      ENABLE_FEATURE(vk13, robustImageAccess, false),
      ENABLE_FEATURE(vk13, pipelineCreationCacheControl, false),
      ENABLE_FEATURE(vk13, shaderDemoteToHelperInvocation, true),
      ENABLE_FEATURE(vk13, shaderZeroInitializeWorkgroupMemory, true),
      ENABLE_FEATURE(vk13, subgroupSizeControl, true),
      ENABLE_FEATURE(vk13, synchronization2, true),

      /* Allows sampling currently bound render targets for client APIs */
      ENABLE_EXT_FEATURE(extAttachmentFeedbackLoopLayout, attachmentFeedbackLoopLayout, false),

      /* Fix some border color jank due to hardware differences */
      ENABLE_EXT_FEATURE(extBorderColorSwizzle, borderColorSwizzle, false),
      ENABLE_EXT_FEATURE(extBorderColorSwizzle, borderColorSwizzleFromImage, false),

      /* Enables client API features */
      ENABLE_EXT(extConservativeRasterization, false),

      /* Legacy feature exposed in client APIs */
      ENABLE_EXT_FEATURE(extCustomBorderColor, customBorderColors, false),
      ENABLE_EXT_FEATURE(extCustomBorderColor, customBorderColorWithoutFormat, false),

      /* Depth clip matches D3D semantics where depth clamp does not */
      ENABLE_EXT_FEATURE(extDepthClipEnable, depthClipEnable, true),

      /* Controls depth bias behaviour with emulated depth formats */
      ENABLE_EXT_FEATURE(extDepthBiasControl, depthBiasControl, false),
      ENABLE_EXT_FEATURE(extDepthBiasControl, leastRepresentableValueForceUnormRepresentation, false),
      ENABLE_EXT_FEATURE(extDepthBiasControl, floatRepresentation, false),
      ENABLE_EXT_FEATURE(extDepthBiasControl, depthBiasExact, false),

      /* Deprecated, used when descriptor heap is unavailable */
      ENABLE_EXT_FEATURE(extDescriptorBuffer, descriptorBuffer, false),

      /* Descriptor heaps for a more efficient binding model */
      ENABLE_EXT_FEATURE(extDescriptorHeap, descriptorHeap, false),

      /* Dynamic state to further improve the graphics_pipeline_library experience */
      ENABLE_EXT_FEATURE(extExtendedDynamicState3, extendedDynamicState3AlphaToCoverageEnable, false),
      ENABLE_EXT_FEATURE(extExtendedDynamicState3, extendedDynamicState3DepthClipEnable, false),
      ENABLE_EXT_FEATURE(extExtendedDynamicState3, extendedDynamicState3RasterizationSamples, false),
      ENABLE_EXT_FEATURE(extExtendedDynamicState3, extendedDynamicState3SampleMask, false),
      ENABLE_EXT_FEATURE(extExtendedDynamicState3, extendedDynamicState3LineRasterizationMode, false),
      ENABLE_EXT_FEATURE(extExtendedDynamicState3, extendedDynamicState3SampleLocationsEnable, false),

      /* Enables client API features */
      ENABLE_EXT_FEATURE(extFragmentShaderInterlock, fragmentShaderSampleInterlock, false),
      ENABLE_EXT_FEATURE(extFragmentShaderInterlock, fragmentShaderPixelInterlock, false),

      /* Windows-only extension to work around driver-side FSE issues */
      ENABLE_EXT(extFullScreenExclusive, false),

      /* Graphics pipeline libraries for stutter-free gameplay */
      ENABLE_EXT_FEATURE(extGraphicsPipelineLibrary, graphicsPipelineLibrary, false),

      /* HDR metadata */
      ENABLE_EXT(extHdrMetadata, false),

      /* Line rasterization features for client APIs */
      ENABLE_EXT_FEATURE(extLineRasterization, rectangularLines,  false),
      ENABLE_EXT_FEATURE(extLineRasterization, smoothLines, false),

      /* Memory budget and priority for improved memory management */
      ENABLE_EXT(extMemoryBudget, false),
      ENABLE_EXT_FEATURE(extMemoryPriority, memoryPriority, false),

      /* Optionally used to batch consecutive draws */
      ENABLE_EXT_FEATURE(extMultiDraw, multiDraw, false),

      /* Legacy cubemap for older client APIs */
      ENABLE_EXT_FEATURE(extNonSeamlessCubeMap, nonSeamlessCubeMap, false),

      /* Enables more dynamic driver-side memory management */
      ENABLE_EXT_FEATURE(extPageableDeviceLocalMemory, pageableDeviceLocalMemory, false),

      /* Robustness, all features effectively required for correctness */
      ENABLE_EXT_FEATURE(extRobustness2, robustBufferAccess2, true),
      ENABLE_EXT_FEATURE(extRobustness2, robustImageAccess2, false),
      ENABLE_EXT_FEATURE(extRobustness2, nullDescriptor, true),

      /* Sample locations, used to "disable" MSAA rendering */
      ENABLE_EXT(extSampleLocations, false),

      /* Shader module identifier, used for pipeline lifetime management in 32-bit */
      ENABLE_EXT_FEATURE(extShaderModuleIdentifier, shaderModuleIdentifier, false),

      /* Stencil export, used both internally and in client APIs */
      ENABLE_EXT(extShaderStencilExport, false),

      /* HDR color space support */
      ENABLE_EXT(extSwapchainColorSpace, false),

      /* Swapchain maintenance, used to implement proper synchronization
       * and dynamic present modes to avoid swapchain recreation */
      ENABLE_EXT_FEATURE(extSwapchainMaintenance1, swapchainMaintenance1, false),

      /* Transform feedback, required for some client APIs */
      ENABLE_EXT_FEATURE(extTransformFeedback, transformFeedback, false),
      ENABLE_EXT_FEATURE(extTransformFeedback, geometryStreams, false),

      /* Vertex attribute divisor, used by client APIs */
      ENABLE_EXT_FEATURE(extVertexAttributeDivisor, vertexAttributeInstanceRateDivisor, false),
      ENABLE_EXT_FEATURE(extVertexAttributeDivisor, vertexAttributeInstanceRateZeroDivisor, false),

      /* External memory features for wine */
      ENABLE_EXT(khrExternalMemoryWin32, false),
      ENABLE_EXT(khrExternalSemaphoreWin32, false),

      /* LOAD_OP_NONE for certain tiler optimizations. Core feature
       * in Vulkan 1.4, so probably supported by everything we need. */
      ENABLE_EXT(khrLoadStoreOpNone, true),

      /* Maintenance features, relied on in various parts of the code */
      ENABLE_EXT_FEATURE(khrMaintenance5, maintenance5, true),
      ENABLE_EXT_FEATURE(khrMaintenance6, maintenance6, true),
      ENABLE_EXT_FEATURE(khrMaintenance7, maintenance7, false),
      ENABLE_EXT_FEATURE(khrMaintenance8, maintenance8, false),
      ENABLE_EXT_FEATURE(khrMaintenance9, maintenance9, false),
      ENABLE_EXT_FEATURE(khrMaintenance10, maintenance10, false),

      /* Dependency for graphics pipeline library */
      ENABLE_EXT(khrPipelineLibrary, true),

      /* Present wait, used for frame pacing and statistics */
      ENABLE_EXT_FEATURE(khrPresentId, presentId, false),
      ENABLE_EXT_FEATURE(khrPresentId2, presentId2, false),
      ENABLE_EXT_FEATURE(khrPresentWait, presentWait, false),
      ENABLE_EXT_FEATURE(khrPresentWait2, presentWait2, false),

      /* Used for shader compilation in addition to regular float_controls features */
      ENABLE_EXT_FEATURE(khrShaderFloatControls2, shaderFloatControls2, false),

      /* Subgroup uniform control flow for some built-in shaders */
      ENABLE_EXT_FEATURE(khrShaderSubgroupUniformControlFlow, shaderSubgroupUniformControlFlow, false),

      /* Untyped pointers, dependency for descriptor heaps */
      ENABLE_EXT_FEATURE(khrShaderUntypedPointers, shaderUntypedPointers, false),

      /* Swapchain, needed for presentation */
      ENABLE_EXT(khrSwapchain, true),

      /* Swapchain maintenance, used to implement proper synchronization
       * and dynamic present modes to avoid swapchain recreation */
      ENABLE_EXT_FEATURE(khrSwapchainMaintenance1, swapchainMaintenance1, false),

      /* Mutable format used to change srgb-ness of swapchain views */
      ENABLE_EXT(khrSwapchainMutableFormat, false),

      /* Use GENERAL layout for everything */
      ENABLE_EXT_FEATURE(khrUnifiedImageLayouts, unifiedImageLayouts, false),

      /* Keyed mutex support in wine */
      ENABLE_EXT(khrWin32KeyedMutex, false),

      /* Reflex support */
      ENABLE_EXT(nvLowLatency2, false),

      /* Raw access chains, improves performance on NV */
      ENABLE_EXT_FEATURE(nvRawAccessChains, shaderRawAccessChains, false),

      /* CUDA interop extensions */
      ENABLE_EXT(nvxBinaryImport, false),
      ENABLE_EXT(nvxImageViewHandle, false),
    }};

    #undef ENABLE_FEATURE
    #undef ENABLE_EXT
    #undef ENABLE_EXT_FEATURE
  }


  Version DxvkDeviceCapabilities::decodeDriverVersion(VkDriverId driverId, uint32_t version) {
    switch (driverId) {
      case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
        return Version(
          (version >> 22) & 0x3ff,
          (version >> 14) & 0x0ff,
          (version >>  6) & 0x0ff);
        break;

      case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
        return Version(version >> 14, version & 0x3fff, 0);

      default:
        return Version(
          VK_API_VERSION_MAJOR(version),
          VK_API_VERSION_MINOR(version),
          VK_API_VERSION_PATCH(version));
    }
  }

}
