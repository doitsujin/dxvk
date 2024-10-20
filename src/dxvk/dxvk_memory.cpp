#include <algorithm>
#include <iomanip>
#include <sstream>

#include "dxvk_device.h"
#include "dxvk_memory.h"
#include "dxvk_sparse.h"

namespace dxvk {

  DxvkResourceBufferViewMap::DxvkResourceBufferViewMap(
          DxvkMemoryAllocator*        allocator,
          VkBuffer                    buffer)
  : m_vkd(allocator->device()->vkd()), m_buffer(buffer),
    m_passBufferUsage(allocator->device()->features().khrMaintenance5.maintenance5) {

  }


  DxvkResourceBufferViewMap::~DxvkResourceBufferViewMap() {
    for (const auto& view : m_views)
      m_vkd->vkDestroyBufferView(m_vkd->device(), view.second, nullptr);
  }


  VkBufferView DxvkResourceBufferViewMap::createBufferView(
    const DxvkBufferViewKey&          key,
          VkDeviceSize                baseOffset) {
    std::lock_guard lock(m_mutex);

    auto entry = m_views.find(key);

    if (entry != m_views.end())
      return entry->second;

    VkBufferUsageFlags2CreateInfoKHR flags = { VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR };
    flags.usage = key.usage;

    VkBufferViewCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
    info.buffer = m_buffer;
    info.format = key.format;
    info.offset = key.offset + baseOffset;
    info.range = key.size;

    if (m_passBufferUsage)
      info.pNext = &flags;

    VkBufferView view = VK_NULL_HANDLE;

    VkResult vr = m_vkd->vkCreateBufferView(
      m_vkd->device(), &info, nullptr, &view);

    if (vr != VK_SUCCESS) {
      throw DxvkError(str::format("Failed to create Vulkan buffer view: ", vr,
        "\n   usage:  0x", std::hex, key.usage,
        "\n   format: ", key.format,
        "\n   offset: ", std::dec, key.offset,
        "\n   size:   ", std::dec, key.size));
    }

    m_views.insert({ key, view });
    return view;
  }




  DxvkResourceImageViewMap::DxvkResourceImageViewMap(
          DxvkMemoryAllocator*        allocator,
          VkImage                     image)
  : m_vkd(allocator->device()->vkd()), m_image(image) {

  }


  DxvkResourceImageViewMap::~DxvkResourceImageViewMap() {
    for (const auto& view : m_views)
      m_vkd->vkDestroyImageView(m_vkd->device(), view.second, nullptr);
  }


  VkImageView DxvkResourceImageViewMap::createImageView(
    const DxvkImageViewKey&           key) {
    std::lock_guard lock(m_mutex);

    auto entry = m_views.find(key);

    if (entry != m_views.end())
      return entry->second;

    VkImageViewUsageCreateInfo usage = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    usage.usage = key.usage;

    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &usage };
    info.image = m_image;
    info.viewType = key.viewType;
    info.format = key.format;
    info.components = key.unpackSwizzle();
    info.subresourceRange.aspectMask = key.aspects;
    info.subresourceRange.baseMipLevel = key.mipIndex;
    info.subresourceRange.levelCount = key.mipCount;
    info.subresourceRange.baseArrayLayer = key.layerIndex;
    info.subresourceRange.layerCount = key.layerCount;

    VkImageView view = VK_NULL_HANDLE;

    VkResult vr = m_vkd->vkCreateImageView(
      m_vkd->device(), &info, nullptr, &view);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create Vulkan image view: ", vr));

    m_views.insert({ key, view });
    return view;
  }




  DxvkResourceAllocation::~DxvkResourceAllocation() {
    if (m_buffer) {
      if (unlikely(m_bufferViews))
        delete m_bufferViews;

      if (unlikely(m_flags.test(DxvkAllocationFlag::OwnsBuffer))) {
        auto vk = m_allocator->device()->vkd();
        vk->vkDestroyBuffer(vk->device(), m_buffer, nullptr);
      }
    }

    if (m_image) {
      if (likely(m_imageViews))
        delete m_imageViews;

      if (likely(m_flags.test(DxvkAllocationFlag::OwnsImage))) {
        auto vk = m_allocator->device()->vkd();
        vk->vkDestroyImage(vk->device(), m_image, nullptr);
      }
    }

    if (unlikely(m_flags.test(DxvkAllocationFlag::OwnsMemory))) {
      auto vk = m_allocator->device()->vkd();
      vk->vkFreeMemory(vk->device(), m_memory, nullptr);

      if (unlikely(m_sparsePageTable))
        delete m_sparsePageTable;
    }
  }


  VkBufferView DxvkResourceAllocation::createBufferView(
    const DxvkBufferViewKey&          key) {
    if (unlikely(!m_bufferViews))
      m_bufferViews = new DxvkResourceBufferViewMap(m_allocator, m_buffer);

    return m_bufferViews->createBufferView(key, m_bufferOffset);
  }


  VkImageView DxvkResourceAllocation::createImageView(
    const DxvkImageViewKey&           key) {
    if (unlikely(!m_imageViews))
      m_imageViews = new DxvkResourceImageViewMap(m_allocator, m_image);

    return m_imageViews->createImageView(key);
  }


  void DxvkResourceAllocation::destroyBufferViews() {
    if (m_bufferViews) {
      delete m_bufferViews;
      m_bufferViews = nullptr;
    }
  }




  DxvkResourceAllocationPool::DxvkResourceAllocationPool() {

  }


  DxvkResourceAllocationPool::~DxvkResourceAllocationPool() {
    auto list = m_next;

    while (list) {
      auto next = list->next;
      list->~StorageList();
      list = next;
    }
  }


  void DxvkResourceAllocationPool::createPool() {
    auto pool = std::make_unique<StoragePool>();
    pool->next = std::move(m_pool);

    for (size_t i = 0; i < pool->objects.size(); i++)
      m_next = new (pool->objects[i].data) StorageList(m_next);

    m_pool = std::move(pool);
  }




  DxvkResourceAllocation* DxvkLocalAllocationCache::allocateFromCache(
          VkDeviceSize                size) {
    uint32_t poolIndex = computePoolIndex(size);
    DxvkResourceAllocation* allocation = m_pools[poolIndex];

    if (!allocation)
      return nullptr;

    m_pools[poolIndex] = allocation->m_next;
    allocation->m_next = nullptr;
    return allocation;
  }


  DxvkResourceAllocation* DxvkLocalAllocationCache::assignCache(
          VkDeviceSize                size,
          DxvkResourceAllocation*     allocation) {
    uint32_t poolIndex = computePoolIndex(size);
    return std::exchange(m_pools[poolIndex], allocation);
  }


  void DxvkLocalAllocationCache::freeCache() {
    if (m_allocator)
      m_allocator->freeLocalCache(this);
  }


  uint32_t DxvkLocalAllocationCache::computePreferredAllocationCount(
          VkDeviceSize                size) {
    uint32_t poolIndex = computePoolIndex(size);
    uint32_t count = (PoolCapacityInBytes / MinSize) >> poolIndex;

    return std::max(count, 1u);
  }


  uint32_t DxvkLocalAllocationCache::computePoolIndex(
          VkDeviceSize                size) {
    return 64u - bit::lzcnt((std::max(size, MinSize) - 1u) / MinSize);
  }


  VkDeviceSize DxvkLocalAllocationCache::computeAllocationSize(
          uint32_t                    index) {
    return MinSize << index;
  }




  DxvkSharedAllocationCache::DxvkSharedAllocationCache(
          DxvkMemoryAllocator*        allocator)
  : m_allocator(allocator) {
    for (uint32_t i = 0; i < m_pools.size(); i++) {
      VkDeviceSize size = DxvkLocalAllocationCache::computeAllocationSize(i);
      m_freeLists[i].capacity = DxvkLocalAllocationCache::computePreferredAllocationCount(size);
    }

    // Initialize unallocated list of lists
    for (uint32_t i = 0u; i < m_lists.size() - 1u; i++)
      m_lists[i].next = i + 1;

    m_nextList = 0;
  }


  DxvkSharedAllocationCache::~DxvkSharedAllocationCache() {
    for (const auto& freeList : m_freeLists)
      m_allocator->freeCachedAllocations(freeList.head);

    for (const auto& list : m_lists)
      m_allocator->freeCachedAllocations(list.head);
  }


  DxvkResourceAllocation* DxvkSharedAllocationCache::getAllocationList(
          VkDeviceSize                allocationSize) {
    uint32_t poolIndex = DxvkLocalAllocationCache::computePoolIndex(allocationSize);

    // If there's a list ready for us, take the whole thing
    std::unique_lock poolLock(m_poolMutex);
    m_numRequests += 1u;

    auto& pool = m_pools[poolIndex];
    int32_t listIndex = pool.listIndex;

    if (listIndex < 0) {
      m_numMisses += 1u;
      return nullptr;
    }

    if (!(--pool.listCount))
      pool.drainTime = high_resolution_clock::now();

    // Extract allocations and mark list as free
    DxvkResourceAllocation* allocation = m_lists[listIndex].head;
    pool.listIndex = m_lists[listIndex].next;

    m_lists[listIndex].head = nullptr;
    m_lists[listIndex].next = m_nextList;

    m_nextList = listIndex;

    m_cacheSize -= PoolCapacityInBytes;
    return allocation;
  }


  DxvkResourceAllocation* DxvkSharedAllocationCache::freeAllocation(
          DxvkResourceAllocation*     allocation) {
    uint32_t poolIndex = DxvkLocalAllocationCache::computePoolIndex(allocation->m_size);

    { std::unique_lock freeLock(m_freeMutex);
      auto& list = m_freeLists[poolIndex];

      allocation->m_next = list.head;
      list.head = allocation;

      if (++list.size < list.capacity)
        return nullptr;

      // Free list is full, try to add it to the list array
      // so that subsequent allocations can use it.
      list.head = nullptr;
      list.size = 0u;
    }

    // Add free list to the pool if possible.
    { std::unique_lock poolLock(m_poolMutex);
      auto& pool = m_pools[poolIndex];

      if (unlikely(m_nextList < 0)) {
        // Cache is currently full, see if we can steal a list from
        // the largest pool. This automatically balances pool sizes
        // under cache pressure.
        uint32_t largestPoolIndex = 0;

        for (uint32_t i = 1; i < PoolCount; i++) {
          if (m_pools[i].listCount > m_pools[largestPoolIndex].listCount)
            largestPoolIndex = i;
        }

        // If the current pool is already (one of) the largest, give up
        // and free the entire list to avoid pools playing ping-pong.
        if (m_pools[largestPoolIndex].listCount == pool.listCount)
          return allocation;

        // Move first list of largest pool to current pool and free any
        // allocations associated with it.
        auto& largestPool = m_pools[largestPoolIndex];
        int32_t listIndex = largestPool.listIndex;

        DxvkResourceAllocation* result = m_lists[listIndex].head;
        largestPool.listIndex = m_lists[listIndex].next;
        largestPool.listCount -= 1u;

        m_lists[listIndex].head = allocation;
        m_lists[listIndex].next = pool.listIndex;

        pool.listIndex = listIndex;
        pool.listCount += 1u;
        return result;
      } else {
        // Otherwise, allocate a fresh list and assign it to the pool
        int32_t listIndex = m_nextList;
        m_nextList = m_lists[listIndex].next;

        m_lists[listIndex].head = allocation;
        m_lists[listIndex].next = pool.listIndex;

        pool.listIndex = listIndex;
        pool.listCount += 1u;

        if ((m_cacheSize += PoolCapacityInBytes) > m_maxCacheSize)
          m_maxCacheSize = m_cacheSize;

        return nullptr;
      }
    }
  }


  DxvkSharedAllocationCacheStats DxvkSharedAllocationCache::getStats() {
    std::unique_lock poolLock(m_poolMutex);

    DxvkSharedAllocationCacheStats result = { };
    result.requestCount = m_numRequests;
    result.missCount = m_numMisses;
    result.size = m_maxCacheSize;

    m_numRequests = 0u;
    m_numMisses = 0u;
    m_maxCacheSize = 0u;
    return result;
  }


  void DxvkSharedAllocationCache::cleanupUnusedFromLockedAllocator(
          high_resolution_clock::time_point time) {
    std::unique_lock poolLock(m_poolMutex);

    for (auto& pool : m_pools) {
      int32_t listIndex = pool.listIndex;

      if (listIndex < 0)
        continue;

      if (time - pool.drainTime >= std::chrono::seconds(1u)) {
        m_allocator->freeCachedAllocationsLocked(m_lists[listIndex].head);

        pool.listIndex = m_lists[listIndex].next;
        pool.listCount -= 1u;
        pool.drainTime = time;

        m_lists[listIndex].head = nullptr;
        m_lists[listIndex].next = m_nextList;

        m_nextList = listIndex;

        m_cacheSize -= PoolCapacityInBytes;
      }
    }
  }




  DxvkMemoryAllocator::DxvkMemoryAllocator(DxvkDevice* device)
  : m_device(device), m_sharingModeInfo(m_device->getSharingMode()) {
    VkPhysicalDeviceMemoryProperties memInfo = m_device->adapter()->memoryProperties();

    m_memTypeCount = memInfo.memoryTypeCount;
    m_memHeapCount = memInfo.memoryHeapCount;

    for (uint32_t i = 0; i < m_memHeapCount; i++) {
      auto& heap = m_memHeaps[i];

      heap.index = i;
      heap.memoryBudget = memInfo.memoryHeaps[i].size;
      heap.properties = memInfo.memoryHeaps[i];
    }

    for (uint32_t i = 0; i < m_memTypeCount; i++) {
      auto& type = m_memTypes[i];

      type.index = i;
      type.properties = memInfo.memoryTypes[i];
      type.heap = &m_memHeaps[type.properties.heapIndex];
      type.heap->memoryTypes |= 1u << i;

      type.devicePool.maxChunkSize = determineMaxChunkSize(type, false);
      type.mappedPool.maxChunkSize = determineMaxChunkSize(type, true);

      // Uncached system memory is going to be used for large temporary allocations
      // during resource creation. Account for that by always using full-sized chunks.
      if ((type.properties.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
       && !(type.properties.propertyFlags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)))
        type.mappedPool.nextChunkSize = type.mappedPool.maxChunkSize;
    }

    determineMemoryTypesWithPropertyFlags();

    if (device->features().core.features.sparseBinding)
      m_sparseMemoryTypes = determineSparseMemoryTypes(device);

    determineBufferUsageFlagsPerMemoryType();

    updateMemoryHeapBudgets();
  }
  
  
  DxvkMemoryAllocator::~DxvkMemoryAllocator() {
    auto vk = m_device->vkd();

    // Destroy shared caches so that any allocations
    // that are still alive get returned to the device
    for (uint32_t i = 0; i < m_memTypeCount; i++) {
      if (m_memTypes[i].sharedCache)
        delete m_memTypes[i].sharedCache;
    }

    // Now that no allocations are alive, we can free chunks
    for (uint32_t i = 0; i < m_memHeapCount; i++)
      freeEmptyChunksInHeap(m_memHeaps[i], VkDeviceSize(-1), high_resolution_clock::time_point());

    // Ensure adapter allocation statistics are consistent
    // when the deivce is being destroyed
    for (uint32_t i = 0; i < m_memHeapCount; i++) {
      m_device->notifyMemoryStats(i,
        -m_adapterHeapStats[i].memoryAllocated,
        -m_adapterHeapStats[i].memoryUsed);
    }
  }
  
  
  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::allocateMemory(
    const VkMemoryRequirements&             requirements,
          VkMemoryPropertyFlags             properties) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    // Ensure the allocation size is also aligned
    VkDeviceSize size = align(requirements.size, requirements.alignment);

    for (auto typeIndex : bit::BitMask(requirements.memoryTypeBits & getMemoryTypeMask(properties))) {
      auto& type = m_memTypes[typeIndex];

      // Use correct memory pool depending on property flags. This way we avoid
      // wasting address space on fallback allocations, or on UMA devices that
      // only expose one memory type.
      auto& selectedPool = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        ? type.mappedPool
        : type.devicePool;

      // Always try to suballocate first, even if the allocation is
      // very large. We will decide what to do if this fails.
      int64_t address = selectedPool.alloc(size, requirements.alignment);

      if (likely(address >= 0))
        return createAllocation(type, selectedPool, address, size);

      // If the memory type is host-visible, try to find an existing chunk
      // in the other memory pool of the memory type and move over.
      if (type.properties.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        auto& oppositePool = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
          ? type.devicePool
          : type.mappedPool;

        int32_t freeChunkIndex = findEmptyChunkInPool(oppositePool,
          size, selectedPool.maxChunkSize);

        if (freeChunkIndex >= 0) {
          uint32_t poolChunkIndex = selectedPool.pageAllocator.addChunk(oppositePool.chunks[freeChunkIndex].memory.size);
          selectedPool.chunks.resize(std::max<size_t>(selectedPool.chunks.size(), poolChunkIndex + 1u));
          selectedPool.chunks[poolChunkIndex] = oppositePool.chunks[freeChunkIndex];

          oppositePool.pageAllocator.removeChunk(freeChunkIndex);
          oppositePool.chunks[freeChunkIndex] = DxvkMemoryChunk();

          mapDeviceMemory(selectedPool.chunks[poolChunkIndex].memory, properties);

          address = selectedPool.alloc(size, requirements.alignment);

          if (likely(address >= 0))
            return createAllocation(type, selectedPool, address, size);
        }
      }

      // If the allocation is very large, use a dedicated allocation instead
      // of creating a new chunk. This way we avoid excessive fragmentation,
      // especially when a multiple such resources are created at once.
      uint32_t minResourcesPerChunk = (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? 1u : 4u;

      // If we're on a mapped memory type and we're about to lose an entire chunk
      // worth of memory to huge resources causing fragmentation, use dedicated
      // allocations anyway and hope that the app doesn't do this every frame.
      if (minResourcesPerChunk == 1u && size > selectedPool.maxChunkSize / 2u
       && type.stats.memoryAllocated - type.stats.memoryUsed + selectedPool.maxChunkSize - size >= selectedPool.maxChunkSize)
        minResourcesPerChunk = 2u;

      if (size * minResourcesPerChunk > selectedPool.maxChunkSize) {
        DxvkDeviceMemory memory = allocateDeviceMemory(type, requirements.size, nullptr);

        if (!memory.memory)
          continue;

        mapDeviceMemory(memory, properties);
        return createAllocation(type, memory);
      }

      // Try to allocate a new chunk that is large enough to hold
      // multiple resources of the type we're tying to allocate.
      VkDeviceSize desiredSize = selectedPool.nextChunkSize;

      while (desiredSize < size * minResourcesPerChunk)
        desiredSize *= 2u;

      if (allocateChunkInPool(type, selectedPool, properties, size, desiredSize)) {
        address = selectedPool.alloc(size, requirements.alignment);
        return createAllocation(type, selectedPool, address, size);
      }
    }

    return nullptr;
  }


  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::allocateDedicatedMemory(
    const VkMemoryRequirements&             requirements,
          VkMemoryPropertyFlags             properties,
    const void*                             next) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkDeviceMemory memory = { };

    for (auto typeIndex : bit::BitMask(requirements.memoryTypeBits & getMemoryTypeMask(properties))) {
      auto& type = m_memTypes[typeIndex];
      memory = allocateDeviceMemory(type, requirements.size, next);

      if (likely(memory.memory != VK_NULL_HANDLE)) {
        mapDeviceMemory(memory, properties);
        return createAllocation(type, memory);
      }
    }

    return nullptr;
  }


  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::createBufferResource(
    const VkBufferCreateInfo&         createInfo,
          VkMemoryPropertyFlags       properties,
          DxvkLocalAllocationCache*   allocationCache) {
    Rc<DxvkResourceAllocation> allocation;

    if (likely(!createInfo.flags)) {
      VkMemoryRequirements memoryRequirements = { };
      memoryRequirements.size = createInfo.size;
      memoryRequirements.alignment = GlobalBufferAlignment;
      memoryRequirements.memoryTypeBits = m_globalBufferMemoryTypes;

      if (unlikely(createInfo.usage & ~m_globalBufferUsageFlags))
        memoryRequirements.memoryTypeBits = findGlobalBufferMemoryTypeMask(createInfo.usage);

      if (likely(memoryRequirements.memoryTypeBits)) {
        // If the given allocation cache supports the memory types and usage
        // flags that we need, try to use it to service this allocation.
        // Only use the allocation cache for mappable allocations since those
        // are expected to happen frequently.
        if (allocationCache && createInfo.size <= DxvkLocalAllocationCache::MaxSize
         && allocationCache->m_memoryTypes && !(allocationCache->m_memoryTypes & ~memoryRequirements.memoryTypeBits)
         && (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
          allocation = allocationCache->allocateFromCache(createInfo.size);

          if (likely(allocation))
            return allocation;

          // If the cache is currently empty for the required allocation size,
          // make sure it's not. This will also initialize the shared caches
          // for any relevant memory pools as necessary.
          if (refillAllocationCache(allocationCache, memoryRequirements, properties))
            return allocationCache->allocateFromCache(createInfo.size);
        }

        // If there is at least one memory type that supports the required
        // buffer usage flags and requested memory properties, suballocate
        // from a global buffer.
        allocation = allocateMemory(memoryRequirements, properties);

        if (likely(allocation && allocation->m_buffer))
          return allocation;

        if (!allocation && (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
          allocation = allocateMemory(memoryRequirements,
            properties & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

          if (likely(allocation && allocation->m_buffer))
            return allocation;
        }

        // If we end up here with an allocation but no buffer, something
        // is weird, but we can keep the allocation around for now.
        if (allocation && !allocation->m_buffer) {
          Logger::err(str::format("Got allocation from memory type ",
            allocation->m_type->index, " without global buffer"));
        }
      }
    }

    // If we can't suballocate from an existing global buffer
    // for any reason, create a dedicated buffer resource.
    auto vk = m_device->vkd();

    VkBuffer buffer = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateBuffer(vk->device(),
      &createInfo, nullptr, &buffer);

    if (vr != VK_SUCCESS) {
      throw DxvkError(str::format("Failed to create buffer: ", vr,
        "\n  size:    ", createInfo.size,
        "\n  usage:   ", std::hex, createInfo.usage,
        "\n  flags:   ", createInfo.flags));
    }

    if (!(createInfo.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)) {
      VkBufferMemoryRequirementsInfo2 requirementInfo = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 };
      requirementInfo.buffer = buffer;

      VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
      vk->vkGetBufferMemoryRequirements2(vk->device(), &requirementInfo, &requirements);

      // If we have an existing global allocation from earlier, make sure it is suitable
      if (!allocation || !(requirements.memoryRequirements.memoryTypeBits & (1u << allocation->m_type->index))
       || (allocation->m_size < requirements.memoryRequirements.size)
       || (allocation->m_address & requirements.memoryRequirements.alignment))
        allocation = allocateMemory(requirements.memoryRequirements, properties);

      if (!allocation && (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        allocation = allocateMemory(requirements.memoryRequirements,
          properties & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      }

      if (!allocation) {
        logMemoryError(requirements.memoryRequirements);
        logMemoryStats();
      }
    } else {
      allocation = createAllocation(new DxvkSparsePageTable(m_device, createInfo, buffer));
    }

    if (!allocation) {
      vk->vkDestroyBuffer(vk->device(), buffer, nullptr);
      return nullptr;
    }

    // Transfer ownership of te Vulkan buffer to the allocation
    // and set up all remaining properties.
    allocation->m_flags.set(DxvkAllocationFlag::OwnsBuffer);
    allocation->m_buffer = buffer;
    allocation->m_bufferOffset = 0u;
    allocation->m_bufferAddress = 0u;

    // Bind memory if the buffer is not sparse
    if (allocation->m_memory) {
      vr = vk->vkBindBufferMemory(vk->device(), allocation->m_buffer,
        allocation->m_memory, allocation->m_address & DxvkPageAllocator::ChunkAddressMask);

      if (vr != VK_SUCCESS) {
        throw DxvkError(str::format("Failed to bind buffer memory: ", vr,
          "\n  size:    ", createInfo.size,
          "\n  usage:   ", std::hex, createInfo.usage,
          "\n  flags:   ", createInfo.flags));
      }
    }

    // Query device address after binding memory, or the address would be invalid
    if (createInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
      allocation->m_bufferAddress = getBufferDeviceAddress(buffer);

    return allocation;
  }


  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::createImageResource(
    const VkImageCreateInfo&          createInfo,
          VkMemoryPropertyFlags       properties,
    const void*                       next) {
    auto vk = m_device->vkd();

    VkImage image = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateImage(vk->device(), &createInfo, nullptr, &image);

    if (vr != VK_SUCCESS) {
      throw DxvkError(str::format("Failed to create image: ", vr,
        "\n  type:    ", createInfo.imageType,
        "\n  format:  ", createInfo.format,
        "\n  extent:  ", createInfo.extent.width, "x", createInfo.extent.height, "x", createInfo.extent.depth,
        "\n  layers:  ", createInfo.arrayLayers,
        "\n  mips:    ", createInfo.mipLevels,
        "\n  samples: ", createInfo.samples));
    }

    // Check memory requirements, including whether or not we need a dedicated allocation
    VkMemoryDedicatedRequirements dedicatedRequirements = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS };

    VkImageMemoryRequirementsInfo2 requirementInfo = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
    requirementInfo.image = image;

    VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicatedRequirements };
    vk->vkGetImageMemoryRequirements2(vk->device(), &requirementInfo, &requirements);

    // For shared resources, we always require a dedicated allocation
    if (next) {
      dedicatedRequirements.requiresDedicatedAllocation = VK_TRUE;
      dedicatedRequirements.prefersDedicatedAllocation = VK_TRUE;
    }

    Rc<DxvkResourceAllocation> allocation;

    if (!(createInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)) {
      // If a dedicated allocation is at least preferred for this resource, try this first
      if (!allocation && dedicatedRequirements.prefersDedicatedAllocation) {
        VkMemoryDedicatedAllocateInfo dedicatedInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, next };
        dedicatedInfo.image = image;

        allocation = allocateDedicatedMemory(requirements.memoryRequirements, properties, &dedicatedInfo);

        // Only retry with a dedicated sysmem allocation if a dedicated allocation
        // is required. Otherwise, we should try to suballocate in device memory.
        if (!allocation && dedicatedRequirements.requiresDedicatedAllocation
        && (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
          allocation = allocateDedicatedMemory(requirements.memoryRequirements,
            properties & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &dedicatedInfo);
        }
      }

      if (!allocation && !dedicatedRequirements.requiresDedicatedAllocation) {
        // Pad alignment as necessary to not overlap tiled and linear memory.
        if (createInfo.tiling == VK_IMAGE_TILING_OPTIMAL) {
          requirements.memoryRequirements.alignment = std::max(
            requirements.memoryRequirements.alignment,
            m_device->properties().core.properties.limits.bufferImageGranularity);
        }

        // Try to suballocate memory and fall back to system memory on error.
        allocation = allocateMemory(requirements.memoryRequirements, properties);

        if (!allocation && (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
          allocation = allocateMemory(requirements.memoryRequirements,
            properties & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }
      }
    } else {
      // Create a sparse page table and determine whether we need to allocate
      // actual memory for the metadata aspect of the image or not.
      auto pageTable = std::make_unique<DxvkSparsePageTable>(m_device, createInfo, image);
      auto pageProperties = pageTable->getProperties();

      if (pageProperties.metadataPageCount) {
        VkMemoryRequirements metadataRequirements = { };
        metadataRequirements.size = SparseMemoryPageSize * pageProperties.metadataPageCount;
        metadataRequirements.alignment = SparseMemoryPageSize;
        metadataRequirements.memoryTypeBits = requirements.memoryRequirements.memoryTypeBits;

        allocation = allocateMemory(metadataRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (!allocation)
          allocation = allocateMemory(metadataRequirements, 0u);

        if (allocation)
          allocation->m_sparsePageTable = pageTable.release();
      } else {
        // Just need a page table, but no memory
        allocation = createAllocation(pageTable.release());
      }
    }

    if (!allocation) {
      vk->vkDestroyImage(vk->device(), image, nullptr);

      logMemoryError(requirements.memoryRequirements);
      logMemoryStats();
      return nullptr;
    }

    // Set up allocation object and bind memory
    allocation->m_flags.set(DxvkAllocationFlag::OwnsImage);
    allocation->m_image = image;

    if (allocation->m_memory) {
      vr = vk->vkBindImageMemory(vk->device(), image, allocation->m_memory,
        allocation->m_address & DxvkPageAllocator::ChunkAddressMask);

      if (vr != VK_SUCCESS) {
        throw DxvkError(str::format("Failed to bind image memory: ", vr,
          "\n  type:    ", createInfo.imageType,
          "\n  format:  ", createInfo.format,
          "\n  extent:  ", createInfo.extent.width, "x", createInfo.extent.height, "x", createInfo.extent.depth,
          "\n  layers:  ", createInfo.arrayLayers,
          "\n  mips:    ", createInfo.mipLevels,
          "\n  samples: ", createInfo.samples));
      }
    }

    return allocation;
  }


  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::createSparsePage() {
    VkMemoryRequirements requirements = { };
    requirements.size = SparseMemoryPageSize;
    requirements.alignment = SparseMemoryPageSize;
    requirements.memoryTypeBits = m_sparseMemoryTypes;

    // Try device memory first, fall back to system memory if that fails.
    // We might get an allocation with a global buffer, just ignore that.
    auto allocation = allocateMemory(requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!allocation)
      allocation = allocateMemory(requirements, 0);

    if (!allocation)
      return nullptr;

    return allocation;
  }


  DxvkLocalAllocationCache DxvkMemoryAllocator::createAllocationCache(
          VkBufferUsageFlags          bufferUsage,
          VkMemoryPropertyFlags       properties) {
    uint32_t memoryTypeMask = m_globalBufferMemoryTypes;

    if (bufferUsage & ~m_globalBufferUsageFlags)
      memoryTypeMask = findGlobalBufferMemoryTypeMask(bufferUsage);

    memoryTypeMask &= getMemoryTypeMask(properties);
    return DxvkLocalAllocationCache(this, memoryTypeMask);
  }


  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::importBufferResource(
    const VkBufferCreateInfo&         createInfo,
    const DxvkBufferImportInfo&       importInfo) {
    Rc<DxvkResourceAllocation> allocation = m_allocationPool.create(this, nullptr);
    allocation->m_flags.set(DxvkAllocationFlag::Imported);
    allocation->m_size = createInfo.size;
    allocation->m_mapPtr = importInfo.mapPtr;
    allocation->m_buffer = importInfo.buffer;
    allocation->m_bufferOffset = importInfo.offset;

    if (createInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
      allocation->m_bufferAddress = getBufferDeviceAddress(importInfo.buffer) + importInfo.offset;

    return allocation;
  }


  Rc<DxvkResourceAllocation> DxvkMemoryAllocator::importImageResource(
    const VkImageCreateInfo&          createInfo,
          VkImage                     imageHandle) {
    Rc<DxvkResourceAllocation> allocation = m_allocationPool.create(this, nullptr);
    allocation->m_flags.set(DxvkAllocationFlag::Imported);
    allocation->m_image = imageHandle;

    return allocation;
  }


  DxvkDeviceMemory DxvkMemoryAllocator::allocateDeviceMemory(
          DxvkMemoryType&       type,
          VkDeviceSize          size,
    const void*                 next) {
    auto vk = m_device->vkd();

    // If global buffers are enabled for this allocation, pad the allocation size
    // to a multiple of the global buffer alignment. This can happen when we create
    // a dedicated allocation for a large resource.
    if (type.bufferUsage && !next)
      size = align(size, GlobalBufferAlignment);

    // Preemptively free some unused allocations to reduce memory waste
    freeEmptyChunksInHeap(*type.heap, size, high_resolution_clock::now());

    VkMemoryAllocateInfo memoryInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, next };
    memoryInfo.allocationSize = size;
    memoryInfo.memoryTypeIndex = type.index;

    // Decide on a memory priority based on the memory type and allocation properties
    VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };

    if (type.properties.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
      if (type.properties.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        // BAR allocation. Give this a low priority since these are typically useful
        // when when placed in system memory.
        priorityInfo.priority = 0.0f;
      } else if (next) {
        // Dedicated allocation, may or may not be a shared resource. Assign this the
        // highest priority since this is expected to be a high-bandwidth resource,
        // such as a render target.
        priorityInfo.priority = 1.0f;
      } else {
        // Standard priority for resource allocations
        priorityInfo.priority = 0.5f;
      }

      if (m_device->features().extMemoryPriority.memoryPriority)
        priorityInfo.pNext = std::exchange(memoryInfo.pNext, &priorityInfo);
    }

    // If buffers can be created on this memory type, also enable the device address bit
    VkMemoryAllocateFlagsInfo memoryFlags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };

    if (type.bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
      memoryFlags.pNext = std::exchange(memoryInfo.pNext, &memoryFlags);
      memoryFlags.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    // Try to allocate memory. If this fails, free any remaining
    // unused memory from the heap and try again.
    DxvkDeviceMemory result = { };
    result.size = size;

    if (vk->vkAllocateMemory(vk->device(), &memoryInfo, nullptr, &result.memory)) {
      freeEmptyChunksInHeap(*type.heap, VkDeviceSize(-1), high_resolution_clock::time_point());

      if (vk->vkAllocateMemory(vk->device(), &memoryInfo, nullptr, &result.memory))
        return DxvkDeviceMemory();
    }

    // Create global buffer if the allocation supports it
    if (type.bufferUsage && !next) {
      VkBuffer buffer = VK_NULL_HANDLE;

      VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
      bufferInfo.size = size;
      bufferInfo.usage = type.bufferUsage;
      m_sharingModeInfo.fill(bufferInfo);

      VkResult status = vk->vkCreateBuffer(vk->device(), &bufferInfo, nullptr, &buffer);

      if (status == VK_SUCCESS) {
        VkBufferMemoryRequirementsInfo2 memInfo = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 };
        memInfo.buffer = buffer;

        VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vk->vkGetBufferMemoryRequirements2(vk->device(), &memInfo, &requirements);

        if ((requirements.memoryRequirements.size == size)
         && (requirements.memoryRequirements.memoryTypeBits & (1u << type.index))) {
          status = vk->vkBindBufferMemory(vk->device(), buffer, result.memory, 0);

          if (status == VK_SUCCESS) {
            result.buffer = buffer;

            if (type.bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
              result.gpuVa = getBufferDeviceAddress(buffer);
          }
        }

        if (!result.buffer)
          vk->vkDestroyBuffer(vk->device(), buffer, nullptr);
      }

      if (!result.buffer) {
        Logger::warn(str::format("Failed to create global buffer:",
          "\n  size:  ", std::dec, size,
          "\n  usage: ", std::hex, type.bufferUsage,
          "\n  type:  ", std::dec, type.index));
      }
    }

    type.stats.memoryAllocated += size;
    return result;
  }


  bool DxvkMemoryAllocator::allocateChunkInPool(
          DxvkMemoryType&       type,
          DxvkMemoryPool&       pool,
          VkMemoryPropertyFlags properties,
          VkDeviceSize          requiredSize,
          VkDeviceSize          desiredSize) {
    // Try to allocate device memory. If the allocation fails, retry with
    // a smaller size until we reach a point where we cannot service the
    // allocation.
    DxvkDeviceMemory chunk = { };

    while (!chunk.memory && desiredSize >= std::max(requiredSize, DxvkMemoryPool::MinChunkSize)) {
      chunk = allocateDeviceMemory(type, desiredSize, nullptr);
      desiredSize /= 2u;
    }

    if (!chunk.memory)
      return false;

    mapDeviceMemory(chunk, properties);

    // If we expect the application to require more memory in the
    // future, increase the chunk size for subsequent allocations.
    if (pool.nextChunkSize < pool.maxChunkSize
     && pool.nextChunkSize <= type.stats.memoryAllocated / 2u)
      pool.nextChunkSize *= 2u;

    // Add the newly created chunk to the pool
    uint32_t chunkIndex = pool.pageAllocator.addChunk(chunk.size);

    pool.chunks.resize(std::max<size_t>(pool.chunks.size(), chunkIndex + 1u));
    pool.chunks[chunkIndex].memory = chunk;
    pool.chunks[chunkIndex].unusedTime = high_resolution_clock::time_point();
    return true;
  }


  DxvkResourceAllocation* DxvkMemoryAllocator::createAllocation(
          DxvkMemoryType&       type,
          DxvkMemoryPool&       pool,
          VkDeviceSize          address,
          VkDeviceSize          size) {
    type.stats.memoryUsed += size;

    uint32_t chunkIndex = address >> DxvkPageAllocator::ChunkAddressBits;

    auto& chunk = pool.chunks[chunkIndex];
    chunk.unusedTime = high_resolution_clock::time_point();

    VkDeviceSize offset = address & DxvkPageAllocator::ChunkAddressMask;

    auto allocation = m_allocationPool.create(this, &type);
    allocation->m_memory = chunk.memory.memory;
    allocation->m_address = address;
    allocation->m_size = size;

    if (chunk.memory.mapPtr)
      allocation->m_mapPtr = reinterpret_cast<char*>(chunk.memory.mapPtr) + offset;

    if (chunk.memory.buffer) {
      allocation->m_buffer = chunk.memory.buffer;
      allocation->m_bufferOffset = offset;
      allocation->m_bufferAddress = chunk.memory.gpuVa
        ? chunk.memory.gpuVa + offset : 0u;
    }

    return allocation;
  }


  DxvkResourceAllocation* DxvkMemoryAllocator::createAllocation(
          DxvkSparsePageTable*  sparsePageTable) {
    auto allocation = m_allocationPool.create(this, nullptr);
    allocation->m_sparsePageTable = sparsePageTable;

    return allocation;
  }


  DxvkResourceAllocation* DxvkMemoryAllocator::createAllocation(
          DxvkMemoryType&       type,
    const DxvkDeviceMemory&     memory) {
    type.stats.memoryUsed += memory.size;

    auto allocation = m_allocationPool.create(this, &type);
    allocation->m_flags.set(DxvkAllocationFlag::OwnsMemory);

    if (memory.buffer)
      allocation->m_flags.set(DxvkAllocationFlag::OwnsBuffer);

    allocation->m_memory = memory.memory;
    allocation->m_address = DedicatedChunkAddress;
    allocation->m_size = memory.size;
    allocation->m_mapPtr = memory.mapPtr;

    allocation->m_buffer = memory.buffer;
    allocation->m_bufferAddress = memory.gpuVa;
    return allocation;
  }


  void DxvkMemoryAllocator::freeDeviceMemory(
          DxvkMemoryType&       type,
          DxvkDeviceMemory      memory) {
    auto vk = m_device->vkd();
    vk->vkDestroyBuffer(vk->device(), memory.buffer, nullptr);
    vk->vkFreeMemory(vk->device(), memory.memory, nullptr);

    type.stats.memoryAllocated -= memory.size;
  }


  void DxvkMemoryAllocator::freeAllocation(
          DxvkResourceAllocation* allocation) {
    if (allocation->m_flags.test(DxvkAllocationFlag::Cacheable)) {
      // Return cacheable allocations to the shared cache
      allocation->destroyBufferViews();

      if (allocation->m_type->sharedCache)
        allocation = allocation->m_type->sharedCache->freeAllocation(allocation);

      // If we get a list of allocations back from the
      // shared cache, free all of them in one go
      freeCachedAllocations(allocation);
    } else {
      std::unique_lock lock(m_mutex);

      if (likely(allocation->m_type)) {
        allocation->m_type->stats.memoryUsed -= allocation->m_size;

        if (unlikely(allocation->m_flags.test(DxvkAllocationFlag::OwnsMemory))) {
          // We free the actual allocation later, just update stats here.
          allocation->m_type->stats.memoryAllocated -= allocation->m_size;
        } else {
          auto& pool = allocation->m_mapPtr
            ? allocation->m_type->mappedPool
            : allocation->m_type->devicePool;

          if (unlikely(pool.free(allocation->m_address, allocation->m_size))) {
            if (freeEmptyChunksInPool(*allocation->m_type, pool, 0, high_resolution_clock::now()))
              updateMemoryHeapStats(allocation->m_type->properties.heapIndex);
          }
        }
      }

      m_allocationPool.free(allocation);
    }
  }


  void DxvkMemoryAllocator::freeLocalCache(
          DxvkLocalAllocationCache* cache) {
    std::unique_lock lock(m_mutex);

    for (size_t i = 0; i < cache->m_pools.size(); i++)
      freeCachedAllocationsLocked(std::exchange(cache->m_pools[i], nullptr));
  }


  void DxvkMemoryAllocator::freeCachedAllocations(
          DxvkResourceAllocation* allocation) {
    if (allocation) {
      std::unique_lock lock(m_mutex);
      freeCachedAllocationsLocked(allocation);
    }
  }


  void DxvkMemoryAllocator::freeCachedAllocationsLocked(
          DxvkResourceAllocation* allocation) {
    while (allocation) {
      auto& pool = allocation->m_mapPtr
        ? allocation->m_type->mappedPool
        : allocation->m_type->devicePool;

      // Cached allocations may have a reference count of 0, but they
      // still own the memory, so make sure to release it here.
      allocation->m_type->stats.memoryUsed -= allocation->m_size;

      if (unlikely(pool.free(allocation->m_address, allocation->m_size))) {
        if (freeEmptyChunksInPool(*allocation->m_type, pool, 0, high_resolution_clock::now()))
          updateMemoryHeapStats(allocation->m_type->properties.heapIndex);
      }

      m_allocationPool.free(std::exchange(allocation, allocation->m_next));
    }
  }


  void DxvkMemoryAllocator::freeEmptyChunksInHeap(
    const DxvkMemoryHeap&       heap,
          VkDeviceSize          allocationSize,
          high_resolution_clock::time_point time) {
    bool freed = false;

    for (auto typeIndex : bit::BitMask(heap.memoryTypes)) {
      auto& type = m_memTypes[typeIndex];

      freed |= freeEmptyChunksInPool(type, type.devicePool, allocationSize, time);
      freed |= freeEmptyChunksInPool(type, type.mappedPool, allocationSize, time);
    }

    if (freed)
      updateMemoryHeapStats(heap.index);
  }


  bool DxvkMemoryAllocator::freeEmptyChunksInPool(
          DxvkMemoryType&       type,
          DxvkMemoryPool&       pool,
          VkDeviceSize          allocationSize,
          high_resolution_clock::time_point time) {
    // Allow for one unused max-size chunk on device-local memory types.
    // For mapped memory allocations, we need to be more lenient since
    // applications will frequently allocate staging buffers or dynamic
    // resources.
    VkDeviceSize maxUnusedMemory = pool.maxChunkSize;

    if (&pool == &type.mappedPool)
      maxUnusedMemory *= 4u;

    // Factor current memory allocation into the decision to free chunks
    VkDeviceSize heapBudget = type.heap->memoryBudget;
    VkDeviceSize heapAllocated = getMemoryStats(type.heap->index).memoryAllocated;

    VkDeviceSize unusedMemory = 0u;

    bool chunkFreed = false;

    for (uint32_t i = 0; i < pool.chunks.size(); i++) {
      DxvkMemoryChunk& chunk = pool.chunks[i];

      if (!chunk.memory.memory || pool.pageAllocator.pagesUsed(i))
        continue;

      // Free the chunk if it is smaller than the current chunk size of
      // the pool, since it is unlikely to be useful for future allocations.
      // Also free if the pending allocation would exceed the heap budget.
      bool shouldFree = chunk.memory.size < pool.nextChunkSize
        || allocationSize + heapAllocated > heapBudget
        || allocationSize > heapBudget;

      // If we still don't free the chunk under these conditions, count it
      // towards unused memory in the current memory pool. Once we exceed
      // the limit, free any empty chunk we encounter.
      if (!shouldFree) {
        unusedMemory += chunk.memory.size;
        shouldFree = unusedMemory > maxUnusedMemory;
      }

      // Free chunks that have not been used in some time, but only free
      // one chunk per iteration. Reset the timer if we already freed one.
      if (!shouldFree && time != high_resolution_clock::time_point()) {
        if (chunk.unusedTime == high_resolution_clock::time_point() || chunkFreed)
          chunk.unusedTime = time;
        else
          shouldFree = time - chunk.unusedTime >= std::chrono::seconds(20);
      }

      if (shouldFree) {
        freeDeviceMemory(type, chunk.memory);
        heapAllocated -= chunk.memory.size;

        chunk = DxvkMemoryChunk();
        pool.pageAllocator.removeChunk(i);

        chunkFreed = true;
      }
    }

    return chunkFreed;
  }


  int32_t DxvkMemoryAllocator::findEmptyChunkInPool(
    const DxvkMemoryPool&       pool,
          VkDeviceSize          minSize,
          VkDeviceSize          maxSize) const {
    for (uint32_t i = 0; i < pool.chunks.size(); i++) {
      const auto& chunk = pool.chunks[i].memory;

      if (chunk.memory && chunk.size >= minSize && chunk.size <= maxSize
       && !pool.pageAllocator.pagesUsed(i))
        return int32_t(i);
    }

    return -1;
  }


  void DxvkMemoryAllocator::mapDeviceMemory(
          DxvkDeviceMemory&     memory,
          VkMemoryPropertyFlags properties) {
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      if (memory.mapPtr)
        return;

      auto vk = m_device->vkd();

      VkResult vr = vk->vkMapMemory(vk->device(),
        memory.memory, 0, memory.size, 0, &memory.mapPtr);

      if (vr != VK_SUCCESS) {
        throw DxvkError(str::format("Failed to map Vulkan memory: ", vr,
          "\n  size: ", memory.size, " bytes"));
      }

      Logger::debug(str::format("Mapped memory region 0x", std::hex,
        reinterpret_cast<uintptr_t>(memory.mapPtr), " - 0x",
        reinterpret_cast<uintptr_t>(memory.mapPtr) + memory.size - 1u));
    } else {
      if (!memory.mapPtr)
        return;

      auto vk = m_device->vkd();
      vk->vkUnmapMemory(vk->device(), memory.memory);

      Logger::debug(str::format("Unmapped memory region 0x", std::hex,
        reinterpret_cast<uintptr_t>(memory.mapPtr), " - 0x",
        reinterpret_cast<uintptr_t>(memory.mapPtr) + memory.size - 1u));

      memory.mapPtr = nullptr;
    }
  }


  bool DxvkMemoryAllocator::refillAllocationCache(
          DxvkLocalAllocationCache*   cache,
    const VkMemoryRequirements&       requirements,
          VkMemoryPropertyFlags       properties) {
    // Ensure that all cached allocations report a power-of-two size.
    // The shared cache implementation currently relies on this.
    VkDeviceSize allocationSize = (VkDeviceSize(-1) >> bit::lzcnt(requirements.size - 1u)) + 1u;
    allocationSize = std::max(allocationSize, DxvkLocalAllocationCache::MinSize);

    // Maximum number of allocations when we miss in the shared cache
    uint32_t allocationCount = DxvkLocalAllocationCache::computePreferredAllocationCount(allocationSize);

    for (auto typeIndex : bit::BitMask(cache->m_memoryTypes)) {
      auto& memoryType = m_memTypes[typeIndex];

      // Initialize shared cache on demand only
      if (unlikely(!memoryType.sharedCache)) {
        std::unique_lock lock(m_mutex);

        if (!memoryType.sharedCache)
          memoryType.sharedCache = new DxvkSharedAllocationCache(this);
      }

      // Try to grab a list of allocations from the shared cache first. If
      // this succeeds, allocating several pages of memory is near instant.
      DxvkResourceAllocation* allocation = memoryType.sharedCache->getAllocationList(allocationSize);

      if (likely(allocation)) {
        allocation = cache->assignCache(allocationSize, allocation);
        freeCachedAllocations(allocation);
        return true;
      }

      // Fill cache with the preferred allocation count of this size category so
      // that subsequent allocations can be handled without locking the allocator.
      DxvkResourceAllocation* head = nullptr;
      DxvkResourceAllocation* tail = nullptr;

      std::unique_lock lock(m_mutex);
      auto& memoryPool = memoryType.mappedPool;

      while (allocationCount) {
        // Try to suballocate from existing chunks, but do not create
        // any new chunks. Let the regular code path handle that case
        // as necessary.
        int64_t address = memoryPool.alloc(allocationSize, requirements.alignment);

        if (address < 0)
          break;

        // Add allocation to the list and mark it as cacheable,
        // so it will get recycled as-is after use.
        allocation = createAllocation(memoryType, memoryPool, address, allocationSize);
        allocation->m_flags.set(DxvkAllocationFlag::Cacheable);

        if (tail) {
          tail->m_next = allocation;
          tail = allocation;
        } else {
          head = allocation;
          tail = allocation;
        }

        allocationCount--;
      }

      if (tail) {
        tail->m_next = cache->assignCache(allocationSize, head);
        return true;
      }
    }

    return false;
  }


  void DxvkMemoryAllocator::getAllocationStatsForPool(
    const DxvkMemoryType&       type,
    const DxvkMemoryPool&       pool,
          DxvkMemoryAllocationStats& stats) {
    auto& typeStats = stats.memoryTypes[type.index];

    for (uint32_t i = 0; i < pool.chunks.size(); i++) {
      if (!pool.chunks[i].memory.memory)
        continue;

      typeStats.chunkCount += 1u;

      auto& chunkStats = stats.chunks.emplace_back();
      chunkStats.capacity = pool.chunks[i].memory.size;
      chunkStats.used = pool.pageAllocator.pagesUsed(i) * DxvkPageAllocator::PageSize;
      chunkStats.pageMaskOffset = stats.pageMasks.size();
      chunkStats.pageCount = pool.pageAllocator.pageCount(i);
      chunkStats.mapped = &pool == &type.mappedPool;

      size_t maskCount = (chunkStats.pageCount + 31u) / 32u;
      stats.pageMasks.resize(chunkStats.pageMaskOffset + maskCount);

      pool.pageAllocator.getPageAllocationMask(i, &stats.pageMasks[chunkStats.pageMaskOffset]);
    }
  }


  VkDeviceSize DxvkMemoryAllocator::determineMaxChunkSize(
    const DxvkMemoryType&       type,
          bool                  mappable) const {
    VkDeviceSize size = DxvkMemoryPool::MaxChunkSize;

    // Prefer smaller chunks for host-visible allocations in order to
    // reduce the amount of address space required. We compensate for
    // the smaller size by allowing more unused memory on these heaps.
    if (mappable)
      size /= env::is32BitHostPlatform() ? 16u : 4u;

    // Ensure that we can at least do 7  allocations to fill
    // the heap. Might be useful on systems with small BAR.
    while (7u * size > type.heap->properties.size)
      size /= 2u;

    // Always use at least the minimum chunk size
    return std::max(size, DxvkMemoryPool::MinChunkSize);
  }


  uint32_t DxvkMemoryAllocator::determineSparseMemoryTypes(
          DxvkDevice*           device) const {
    auto vk = device->vkd();

    VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
    uint32_t typeMask = ~0u;

    // Create sparse dummy buffer to find available memory types
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.flags        = VK_BUFFER_CREATE_SPARSE_BINDING_BIT
                            | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
                            | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
    bufferInfo.size         = 65536;
    bufferInfo.usage        = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                            | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                            | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                            | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
                            | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                            | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    m_sharingModeInfo.fill(bufferInfo);

    if (getBufferMemoryRequirements(bufferInfo, requirements))
      typeMask &= requirements.memoryRequirements.memoryTypeBits;

    // Create sparse dummy image to find available memory types
    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags         = VK_IMAGE_CREATE_SPARSE_BINDING_BIT
                            | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT
                            | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent        = { 256, 256, 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_STORAGE_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (getImageMemoryRequirements(imageInfo, requirements))
      typeMask &= requirements.memoryRequirements.memoryTypeBits;

    Logger::log(typeMask ? LogLevel::Info : LogLevel::Error,
      str::format("Memory type mask for sparse resources: 0x", std::hex, typeMask));
    return typeMask;
  }


  void DxvkMemoryAllocator::determineBufferUsageFlagsPerMemoryType() {
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                             | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                             | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                             | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                             | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

    // Lock storage texel buffer usage to maintenance5 support since we will
    // otherwise not be able to legally use formats that support one type of
    // texel buffer but not the other. Also lock index buffer usage since we
    // cannot explicitly specify a buffer range otherwise.
    if (m_device->features().khrMaintenance5.maintenance5) {
      flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            |  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }

    if (m_device->features().extTransformFeedback.transformFeedback) {
      flags |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT
            |  VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
    }

    if (m_device->features().vk12.bufferDeviceAddress)
      flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // Check which individual flags are supported on each memory type. This is a
    // bit dodgy since the spec technically does not require a combination of flags
    // to be supported, but we need to be robust around buffer creation anyway.
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = 65536;
    m_sharingModeInfo.fill(bufferInfo);

    VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

    while (flags) {
      VkBufferCreateFlags flag = flags & -flags;

      bufferInfo.usage = flag
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

      if (getBufferMemoryRequirements(bufferInfo, requirements)) {
        uint32_t typeMask = requirements.memoryRequirements.memoryTypeBits;

        while (typeMask) {
          uint32_t type = bit::tzcnt(typeMask);

          if (type < m_memTypeCount)
            m_memTypes.at(type).bufferUsage |= bufferInfo.usage;

          typeMask &= typeMask - 1;
        }
      }

      flags &= ~flag;
    }

    // Only use a minimal set of usage flags for the global buffer if the
    // full combination of flags is not supported for whatever reason.
    m_globalBufferUsageFlags = ~0u;
    m_globalBufferMemoryTypes = 0u;

    for (uint32_t i = 0; i < m_memTypeCount; i++) {
      bufferInfo.usage = m_memTypes[i].bufferUsage;

      if (!getBufferMemoryRequirements(bufferInfo, requirements)
       || !(requirements.memoryRequirements.memoryTypeBits & (1u << i))) {
        m_memTypes[i].bufferUsage &= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                  |  VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                  |  VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      }

      if (m_memTypes[i].bufferUsage) {
        m_globalBufferUsageFlags &= m_memTypes[i].bufferUsage;
        m_globalBufferMemoryTypes |= 1u << i;
      }
    }

    Logger::info(str::format("Memory type mask for buffer resources: "
      "0x", std::hex, m_globalBufferMemoryTypes, ", usage: 0x", m_globalBufferUsageFlags));
  }


  void DxvkMemoryAllocator::determineMemoryTypesWithPropertyFlags() {
    // Initialize look-up table for memory type masks based on required property
    // flags. This lets us avoid iterating over unsupported memory types
    for (uint32_t i = 0; i < m_memTypesByPropertyFlags.size(); i++) {
      VkMemoryPropertyFlags flags = VkMemoryPropertyFlags(i);
      uint32_t mask = 0u;

      for (uint32_t j = 0; j < m_memTypeCount; j++) {
        VkMemoryPropertyFlags typeFlags = m_memTypes[j].properties.propertyFlags;

        if ((typeFlags & flags) != flags)
          continue;

        // Do not include device-local memory types if a non-device
        // local one exists with the same required propery flags.
        if (mask && !(flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
         && (typeFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
          continue;

        mask |= 1u << j;
      }

      m_memTypesByPropertyFlags[i] = mask;
    }

    // If there is no cached coherent memory type, reuse the uncached
    // one. This is likely slow, but API front-ends are relying on it.
    uint32_t hostCachedIndex = uint32_t(
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    uint32_t hostCoherentIndex = uint32_t(
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!m_memTypesByPropertyFlags[hostCachedIndex])
      m_memTypesByPropertyFlags[hostCachedIndex] = m_memTypesByPropertyFlags[hostCoherentIndex];
  }


  DxvkMemoryStats DxvkMemoryAllocator::getMemoryStats(uint32_t heap) const {
    DxvkMemoryStats result = { };

    for (auto typeIndex : bit::BitMask(m_memHeaps[heap].memoryTypes)) {
      const auto& type = m_memTypes[typeIndex];

      result.memoryAllocated += type.stats.memoryAllocated;
      result.memoryUsed += type.stats.memoryUsed;
    }

    result.memoryBudget = m_memHeaps[heap].memoryBudget;
    return result;
  }


  void DxvkMemoryAllocator::getAllocationStats(DxvkMemoryAllocationStats& stats) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    stats.chunks.clear();
    stats.pageMasks.clear();

    for (uint32_t i = 0; i < m_memTypeCount; i++) {
      const auto& typeInfo = m_memTypes[i];
      auto& typeStats = stats.memoryTypes[i];

      typeStats.properties = typeInfo.properties;
      typeStats.allocated = typeInfo.stats.memoryAllocated;
      typeStats.used = typeInfo.stats.memoryUsed;
      typeStats.chunkIndex = stats.chunks.size();
      typeStats.chunkCount = 0u;

      getAllocationStatsForPool(typeInfo, typeInfo.devicePool, stats);
      getAllocationStatsForPool(typeInfo, typeInfo.mappedPool, stats);
    }
  }


  DxvkSharedAllocationCacheStats DxvkMemoryAllocator::getAllocationCacheStats() const {
    DxvkSharedAllocationCacheStats result = { };

    for (uint32_t i = 0; i < m_memTypeCount; i++) {
      const auto& type = m_memTypes[i];

      if (type.sharedCache) {
        DxvkSharedAllocationCacheStats stats = type.sharedCache->getStats();
        result.requestCount += stats.requestCount;
        result.missCount += stats.missCount;
        result.size += stats.size;
      }
    }

    return result;
  }


  bool DxvkMemoryAllocator::getBufferMemoryRequirements(
    const VkBufferCreateInfo&     createInfo,
          VkMemoryRequirements2&  memoryRequirements) const {
    auto vk = m_device->vkd();

    if (m_device->features().vk13.maintenance4) {
      VkDeviceBufferMemoryRequirements info = { VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS };
      info.pCreateInfo = &createInfo;

      vk->vkGetDeviceBufferMemoryRequirements(vk->device(), &info, &memoryRequirements);
      return true;
    } else {
      VkBufferMemoryRequirementsInfo2 info = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 };
      VkResult vr = vk->vkCreateBuffer(vk->device(), &createInfo, nullptr, &info.buffer);

      if (vr != VK_SUCCESS)
        return false;

      vk->vkGetBufferMemoryRequirements2(vk->device(), &info, &memoryRequirements);
      vk->vkDestroyBuffer(vk->device(), info.buffer, nullptr);
      return true;
    }
  }


  bool DxvkMemoryAllocator::getImageMemoryRequirements(
    const VkImageCreateInfo&      createInfo,
          VkMemoryRequirements2&  memoryRequirements) const {
    auto vk = m_device->vkd();

    if (m_device->features().vk13.maintenance4) {
      VkDeviceImageMemoryRequirements info = { VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS };
      info.pCreateInfo = &createInfo;

      vk->vkGetDeviceImageMemoryRequirements(vk->device(), &info, &memoryRequirements);
      return true;
    } else {
      VkImageMemoryRequirementsInfo2 info = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 };
      VkResult vr = vk->vkCreateImage(vk->device(), &createInfo, nullptr, &info.image);

      if (vr != VK_SUCCESS)
        return false;

      vk->vkGetImageMemoryRequirements2(vk->device(), &info, &memoryRequirements);
      vk->vkDestroyImage(vk->device(), info.image, nullptr);
      return true;
    }
  }


  VkDeviceAddress DxvkMemoryAllocator::getBufferDeviceAddress(VkBuffer buffer) const {
    auto vk = m_device->vkd();

    VkBufferDeviceAddressInfo bdaInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    bdaInfo.buffer = buffer;

    return vk->vkGetBufferDeviceAddress(vk->device(), &bdaInfo);
  }


  void DxvkMemoryAllocator::logMemoryError(const VkMemoryRequirements& req) const {
    std::stringstream sstr;
    sstr << "DxvkMemoryAllocator: Memory allocation failed" << std::endl
         << "  Size:      " << req.size << std::endl
         << "  Alignment: " << req.alignment << std::endl
         << "  Mem types: ";

    uint32_t memTypes = req.memoryTypeBits;

    while (memTypes) {
      uint32_t index = bit::tzcnt(memTypes);
      sstr << index;

      if ((memTypes &= memTypes - 1))
        sstr << ",";
      else
        sstr << std::endl;
    }

    Logger::err(sstr.str());
  }


  void DxvkMemoryAllocator::logMemoryStats() const {
    DxvkAdapterMemoryInfo memHeapInfo = m_device->adapter()->getMemoryHeapInfo();

    std::stringstream sstr;
    sstr << "Heap  Size (MiB)  Allocated   Used        Reserved    Budget" << std::endl;

    for (uint32_t i = 0; i < m_memHeapCount; i++) {
      DxvkMemoryStats stats = getMemoryStats(i);

      sstr << std::setw(2) << i << ":   "
           << std::setw(6) << (m_memHeaps[i].properties.size >> 20) << "      "
           << std::setw(6) << (stats.memoryAllocated >> 20) << "      "
           << std::setw(6) << (stats.memoryUsed >> 20) << "      ";

      if (m_device->features().extMemoryBudget) {
        sstr << std::setw(6) << (memHeapInfo.heaps[i].memoryAllocated >> 20) << "      "
             << std::setw(6) << (memHeapInfo.heaps[i].memoryBudget >> 20) << "      " << std::endl;
      } else {
        sstr << " n/a         n/a" << std::endl;
      }
    }

    Logger::err(sstr.str());
  }


  uint32_t DxvkMemoryAllocator::getMemoryTypeMask(
          VkMemoryPropertyFlags properties) const {
    return m_memTypesByPropertyFlags[uint32_t(properties) % uint32_t(m_memTypesByPropertyFlags.size())];
  }


  uint32_t DxvkMemoryAllocator::findGlobalBufferMemoryTypeMask(
          VkBufferUsageFlags    usage) const {
    // Iterate over all candidate memory types as a fallback in case
    // the device has memory types with limited buffer support.
    uint32_t mask = m_globalBufferMemoryTypes;

    for (auto typeIndex : bit::BitMask(mask)) {
      if (usage & ~m_memTypes[typeIndex].bufferUsage)
        mask ^= 1u << typeIndex;
    }

    return mask;
  }


  void DxvkMemoryAllocator::updateMemoryHeapBudgets() {
    if (!m_device->features().extMemoryBudget)
      return;

    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
    VkPhysicalDeviceMemoryProperties2 memInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &memBudget };

    auto vki = m_device->adapter()->vki();
    vki->vkGetPhysicalDeviceMemoryProperties2(m_device->adapter()->handle(), &memInfo);

    for (uint32_t i = 0; i < m_memHeapCount; i++) {
      if (memBudget.heapBudget[i])
        m_memHeaps[i].memoryBudget = std::min(memBudget.heapBudget[i], m_memHeaps[i].properties.size);
    }
  }


  void DxvkMemoryAllocator::updateMemoryHeapStats(uint32_t heapIndex) {
    DxvkMemoryStats stats = getMemoryStats(heapIndex);

    m_device->notifyMemoryStats(heapIndex,
      stats.memoryAllocated - m_adapterHeapStats[heapIndex].memoryAllocated,
      stats.memoryUsed - m_adapterHeapStats[heapIndex].memoryUsed);

    m_adapterHeapStats[heapIndex] = stats;
  }


  void DxvkMemoryAllocator::performTimedTasks() {
    static constexpr auto Interval = std::chrono::seconds(1u);

    // This function shouldn't be called concurrently, so checking and
    // updating the deadline is fine without taking the global lock
    auto currentTime = high_resolution_clock::now();

    if (m_taskDeadline != high_resolution_clock::time_point()
     && m_taskDeadline > currentTime)
      return;

    if (m_taskDeadline == high_resolution_clock::time_point()
     || m_taskDeadline + Interval <= currentTime)
      m_taskDeadline = currentTime + Interval;
    else
      m_taskDeadline = m_taskDeadline + Interval;

    std::unique_lock lock(m_mutex);
    performTimedTasksLocked(currentTime);
  }


  void DxvkMemoryAllocator::performTimedTasksLocked(high_resolution_clock::time_point currentTime) {
    // Re-query current memory budgets
    updateMemoryHeapBudgets();

    // Periodically free unused memory chunks and update
    // memory allocation statistics for the adapter.
    for (uint32_t i = 0; i < m_memHeapCount; i++)
      freeEmptyChunksInHeap(m_memHeaps[i], 0, currentTime);

    // Periodically clean up unused cached allocations
    for (uint32_t i = 0; i < m_memTypeCount; i++) {
      if (m_memTypes[i].sharedCache)
        m_memTypes[i].sharedCache->cleanupUnusedFromLockedAllocator(currentTime);
    }
  }

}
