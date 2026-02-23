#include <cstring>
#include <unordered_set>

#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {

  DxvkDeviceQueue getDeviceQueue(const Rc<vk::DeviceFn>& vkd, const DxvkDeviceCapabilities& caps, DxvkDeviceQueueIndex queue) {
    DxvkDeviceQueue result = { };
    result.queueFamily = queue.family;
    result.queueIndex = queue.index;

    if (queue.family != VK_QUEUE_FAMILY_IGNORED) {
      result.properties = caps.getQueueProperties(queue.family);
      vkd->vkGetDeviceQueue(vkd->device(), queue.family, queue.index, &result.queueHandle);
    }

    return result;
  }


  DxvkAdapter::DxvkAdapter(
          DxvkInstance&       instance,
          VkPhysicalDevice    handle)
  : m_instance      (&instance),
    m_handle        (handle),
    m_capabilities  (instance, handle, nullptr) {
    const auto& properties = m_capabilities.getProperties();

    if (properties.vk11.deviceLUIDValid) {
      D3DKMT_OPENADAPTERFROMLUID open = { };
      memcpy(&open.AdapterLuid, properties.vk11.deviceLUID, sizeof(open.AdapterLuid));

      if (D3DKMTOpenAdapterFromLuid(&open))
        Logger::warn("Failed to open D3DKMT adapter");
      else
        m_kmtLocal = open.hAdapter;
    }
  }
  
  
  DxvkAdapter::~DxvkAdapter() {
    if (m_kmtLocal) {
      D3DKMT_CLOSEADAPTER close = { };
      close.hAdapter = m_kmtLocal;
      D3DKMTCloseAdapter(&close);
    }
  }


  Rc<vk::InstanceFn> DxvkAdapter::vki() const {
    return m_instance->vki();
  }

  
  bool DxvkAdapter::isCompatible(std::string& error) {
    std::array<char, 1024u> message = { };

    if (m_capabilities.isSuitable(message.size(), message.data()))
      return true;

    error = std::string(message.data());
    return false;
  }


  DxvkAdapterMemoryInfo DxvkAdapter::getMemoryHeapInfo() const {
    bool hasMemoryBudget = m_capabilities.getFeatures().extMemoryBudget;

    auto vk = m_instance->vki();

    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    VkPhysicalDeviceMemoryProperties2 memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    memProps.pNext = hasMemoryBudget ? &memBudget : nullptr;

    vk->vkGetPhysicalDeviceMemoryProperties2(m_handle, &memProps);
    
    DxvkAdapterMemoryInfo info = { };
    info.heapCount = memProps.memoryProperties.memoryHeapCount;

    for (uint32_t i = 0; i < info.heapCount; i++) {
      info.heaps[i].heapFlags = memProps.memoryProperties.memoryHeaps[i].flags;
      info.heaps[i].heapSize = memProps.memoryProperties.memoryHeaps[i].size;

      if (hasMemoryBudget) {
        // Handle DXVK's memory allocations separately so that
        // freeing  resources actually is visible to applications.
        VkDeviceSize allocated = m_memoryStats[i].allocated.load();
        VkDeviceSize used = m_memoryStats[i].used.load();

        info.heaps[i].memoryBudget    = memBudget.heapBudget[i];
        info.heaps[i].memoryAllocated = std::max(memBudget.heapUsage[i], allocated) - allocated + used;
      } else {
        info.heaps[i].memoryBudget    = memProps.memoryProperties.memoryHeaps[i].size;
        info.heaps[i].memoryAllocated = m_memoryStats[i].used.load();
      }
    }

    return info;
  }


  DxvkFormatFeatures DxvkAdapter::getFormatFeatures(VkFormat format) const {
    auto vk = m_instance->vki();

    VkFormatProperties3 properties3 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3 };
    VkFormatProperties2 properties2 = { VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2, &properties3 };
    vk->vkGetPhysicalDeviceFormatProperties2(m_handle, format, &properties2);

    DxvkFormatFeatures result;
    result.optimal = properties3.optimalTilingFeatures;
    result.linear  = properties3.linearTilingFeatures;
    result.buffer  = properties3.bufferFeatures;
    return result;
  }


  std::optional<DxvkFormatLimits> DxvkAdapter::getFormatLimits(
    const DxvkFormatQuery&          query) const {
    auto vk = m_instance->vki();

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

    VkResult vr = vk->vkGetPhysicalDeviceImageFormatProperties2(m_handle, &info, &properties);

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


  void DxvkAdapter::enableExtensions(const DxvkExtensionList& extensions) {
    for (const auto& ext : extensions)
      m_extraExtensions.push_back(ext);
  }


  Rc<DxvkDevice> DxvkAdapter::createDevice() {
    auto vk = m_instance->vki();

    Logger::info("Creating device:");
    m_capabilities.logDeviceInfo();

    // Get device features to enable
    size_t featureBlobSize = 0u;
    m_capabilities.queryDeviceFeatures(&featureBlobSize, nullptr);

    std::vector<char> featureBlob(featureBlobSize);
    m_capabilities.queryDeviceFeatures(&featureBlobSize, featureBlob.data());

    auto features = reinterpret_cast<const VkPhysicalDeviceFeatures2*>(featureBlob.data());

    // Get extension list and add extra extensions
    uint32_t extensionCount = 0u;
    m_capabilities.queryDeviceExtensions(&extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    m_capabilities.queryDeviceExtensions(&extensionCount, extensions.data());

    for (const auto& extra : m_extraExtensions) {
      bool found = false;

      for (const auto& enabled : extensions) {
        if ((found = !std::strncmp(extra.extensionName, enabled.extensionName, sizeof(enabled.extensionName))))
          break;
      }

      if (!found)
        extensions.push_back(extra);
    }

    // Create extension list that we can pass to Vulkan
    std::vector<const char*> extensionNames;
    extensionNames.reserve(extensions.size());

    for (const auto& ext : extensions)
      extensionNames.push_back(ext.extensionName);

    // Query queue infos
    DxvkDeviceQueueMapping queueMapping = m_capabilities.getQueueMapping();

    uint32_t queueCount = { };
    m_capabilities.queryDeviceQueues(&queueCount, nullptr);

    std::vector<VkDeviceQueueCreateInfo> queues(queueCount, { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO });
    m_capabilities.queryDeviceQueues(&queueCount, queues.data());

    uint32_t priorityCount = 0u;

    for (const auto& q : queues)
      priorityCount += q.queueCount;

    std::vector<float> queuePriorities(priorityCount);

    uint32_t priorityIndex = 0u;

    for (auto& q : queues) {
      q.pQueuePriorities = &queuePriorities[priorityIndex];
      priorityIndex += q.queueCount;
    }

    m_capabilities.queryDeviceQueues(&queueCount, queues.data());

    // Create the actual Vulkan device
    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.pNext = features->pNext;
    deviceInfo.queueCreateInfoCount = queues.size();
    deviceInfo.pQueueCreateInfos = queues.data();
    deviceInfo.enabledExtensionCount = extensionNames.size();
    deviceInfo.ppEnabledExtensionNames = extensionNames.data();
    deviceInfo.pEnabledFeatures = &features->features;

    VkDevice device = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateDevice(m_handle, &deviceInfo, nullptr, &device);

    if (vr)
      throw DxvkError(str::format("Failed to create Vulkan device: ", vr));

    Rc<vk::DeviceFn> vkd = new vk::DeviceFn(vk, true, device);

    DxvkDeviceQueueSet deviceQueues = { };
    deviceQueues.graphics = getDeviceQueue(vkd, m_capabilities, queueMapping.graphics);
    deviceQueues.transfer = getDeviceQueue(vkd, m_capabilities, queueMapping.transfer);
    deviceQueues.sparse   = getDeviceQueue(vkd, m_capabilities, queueMapping.sparse);

    return new DxvkDevice(m_instance, this, vkd, m_capabilities.getFeatures(), deviceQueues, DxvkQueueCallback());
  }


  Rc<DxvkDevice> DxvkAdapter::importDevice(
    const DxvkDeviceImportInfo& args) {
    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = args.queueFamily;
    queueInfo.queueCount = 1u;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.pNext = args.features;
    deviceInfo.enabledExtensionCount = args.extensionCount;
    deviceInfo.ppEnabledExtensionNames = args.extensionNames;
    deviceInfo.queueCreateInfoCount = 1u;
    deviceInfo.pQueueCreateInfos = &queueInfo;

    DxvkDeviceCapabilities importCaps(*m_instance, m_handle, &deviceInfo);

    Logger::info("Importing device:");
    importCaps.logDeviceInfo();

    DxvkDeviceQueueMapping queueMapping = importCaps.getQueueMapping();

    Rc<vk::DeviceFn> vkd = new vk::DeviceFn(m_instance->vki(), false, args.device);

    DxvkDeviceQueueSet deviceQueues = { };
    deviceQueues.graphics = getDeviceQueue(vkd, importCaps, queueMapping.graphics);
    deviceQueues.transfer = getDeviceQueue(vkd, importCaps, queueMapping.transfer);
    deviceQueues.sparse   = getDeviceQueue(vkd, importCaps, queueMapping.sparse);

    return new DxvkDevice(m_instance, this, vkd, importCaps.getFeatures(), deviceQueues, args.queueCallback);
  }


  void DxvkAdapter::notifyMemoryStats(
          uint32_t            heap,
          int64_t             allocated,
          int64_t             used) {
    if (heap < m_memoryStats.size()) {
      m_memoryStats[heap].allocated += allocated;
      m_memoryStats[heap].used += used;
    }
  }


  bool DxvkAdapter::matchesDriver(
          VkDriverIdKHR       driver,
          Version             minVer,
          Version             maxVer) const {
    const auto& properties = m_capabilities.getProperties();
    bool driverMatches = driver == properties.vk12.driverID;

    if (minVer) driverMatches &= properties.driverVersion >= minVer;
    if (maxVer) driverMatches &= properties.driverVersion <  maxVer;

    return driverMatches;
  }


  bool DxvkAdapter::matchesDriver(
          VkDriverIdKHR       driver) const {
    const auto& properties = m_capabilities.getProperties();
    return driver == properties.vk12.driverID;
  }


  bool DxvkAdapter::isUnifiedMemoryArchitecture() const {
    auto memory = this->memoryProperties();
    bool result = true;

    for (uint32_t i = 0; i < memory.memoryHeapCount; i++)
      result = result && (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);

    return result;
  }

}
