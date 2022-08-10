#include <algorithm>

#include "dxvk_device.h"
#include "dxvk_memory.h"

namespace dxvk {
  
  DxvkMemory::DxvkMemory() { }
  DxvkMemory::DxvkMemory(
          DxvkMemoryAllocator*  alloc,
          DxvkMemoryChunk*      chunk,
          DxvkMemoryType*       type,
          VkDeviceMemory        memory,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          void*                 mapPtr)
  : m_alloc   (alloc),
    m_chunk   (chunk),
    m_type    (type),
    m_memory  (memory),
    m_offset  (offset),
    m_length  (length),
    m_mapPtr  (mapPtr) { }
  
  
  DxvkMemory::DxvkMemory(DxvkMemory&& other)
  : m_alloc   (std::exchange(other.m_alloc,  nullptr)),
    m_chunk   (std::exchange(other.m_chunk,  nullptr)),
    m_type    (std::exchange(other.m_type,   nullptr)),
    m_memory  (std::exchange(other.m_memory, VkDeviceMemory(VK_NULL_HANDLE))),
    m_offset  (std::exchange(other.m_offset, 0)),
    m_length  (std::exchange(other.m_length, 0)),
    m_mapPtr  (std::exchange(other.m_mapPtr, nullptr)) { }
  
  
  DxvkMemory& DxvkMemory::operator = (DxvkMemory&& other) {
    this->free();
    m_alloc   = std::exchange(other.m_alloc,  nullptr);
    m_chunk   = std::exchange(other.m_chunk,  nullptr);
    m_type    = std::exchange(other.m_type,   nullptr);
    m_memory  = std::exchange(other.m_memory, VkDeviceMemory(VK_NULL_HANDLE));
    m_offset  = std::exchange(other.m_offset, 0);
    m_length  = std::exchange(other.m_length, 0);
    m_mapPtr  = std::exchange(other.m_mapPtr, nullptr);
    return *this;
  }
  
  
  DxvkMemory::~DxvkMemory() {
    this->free();
  }
  
  
  void DxvkMemory::free() {
    if (m_alloc != nullptr)
      m_alloc->free(*this);
  }
  

  DxvkMemoryChunk::DxvkMemoryChunk(
          DxvkMemoryAllocator*  alloc,
          DxvkMemoryType*       type,
          DxvkDeviceMemory      memory,
          DxvkMemoryFlags       hints)
  : m_alloc(alloc), m_type(type), m_memory(memory), m_hints(hints) {
    // Mark the entire chunk as free
    m_freeList.push_back(FreeSlice { 0, memory.memSize });
  }
  
  
  DxvkMemoryChunk::~DxvkMemoryChunk() {
    // This call is technically not thread-safe, but it
    // doesn't need to be since we don't free chunks
    m_alloc->freeDeviceMemory(m_type, m_memory);
  }
  
  
  DxvkMemory DxvkMemoryChunk::alloc(
          VkMemoryPropertyFlags flags,
          VkDeviceSize          size,
          VkDeviceSize          align,
          DxvkMemoryFlags       hints) {
    // Property flags must be compatible. This could
    // be refined a bit in the future if necessary.
    if (m_memory.memFlags != flags || !checkHints(hints))
      return DxvkMemory();
    
    // If the chunk is full, return
    if (m_freeList.size() == 0)
      return DxvkMemory();
    
    // Select the slice to allocate from in a worst-fit
    // manner. This may help keep fragmentation low.
    auto bestSlice = m_freeList.begin();
    
    for (auto slice = m_freeList.begin(); slice != m_freeList.end(); slice++) {
      if (slice->length == size) {
        bestSlice = slice;
        break;
      } else if (slice->length > bestSlice->length) {
        bestSlice = slice;
      }
    }
    
    // We need to align the allocation to the requested alignment
    const VkDeviceSize sliceStart = bestSlice->offset;
    const VkDeviceSize sliceEnd   = bestSlice->offset + bestSlice->length;
    
    const VkDeviceSize allocStart = dxvk::align(sliceStart,        align);
    const VkDeviceSize allocEnd   = dxvk::align(allocStart + size, align);
    
    if (allocEnd > sliceEnd)
      return DxvkMemory();
    
    // We can use this slice, but we'll have to add
    // the unused parts of it back to the free list.
    m_freeList.erase(bestSlice);
    
    if (allocStart != sliceStart)
      m_freeList.push_back({ sliceStart, allocStart - sliceStart });
    
    if (allocEnd != sliceEnd)
      m_freeList.push_back({ allocEnd, sliceEnd - allocEnd });
    
    // Create the memory object with the aligned slice
    return DxvkMemory(m_alloc, this, m_type,
      m_memory.memHandle, allocStart, allocEnd - allocStart,
      reinterpret_cast<char*>(m_memory.memPointer) + allocStart);
  }
  
  
  void DxvkMemoryChunk::free(
          VkDeviceSize  offset,
          VkDeviceSize  length) {
    // Remove adjacent entries from the free list and then add
    // a new slice that covers all those entries. Without doing
    // so, the slice could not be reused for larger allocations.
    auto curr = m_freeList.begin();
    
    while (curr != m_freeList.end()) {
      if (curr->offset == offset + length) {
        length += curr->length;
        curr = m_freeList.erase(curr);
      } else if (curr->offset + curr->length == offset) {
        offset -= curr->length;
        length += curr->length;
        curr = m_freeList.erase(curr);
      } else {
        curr++;
      }
    }
    
    m_freeList.push_back({ offset, length });
  }
  
  
  bool DxvkMemoryChunk::isEmpty() const {
    return m_freeList.size() == 1
        && m_freeList[0].length == m_memory.memSize;
  }


  bool DxvkMemoryChunk::isCompatible(const Rc<DxvkMemoryChunk>& other) const {
    return other->m_memory.memFlags == m_memory.memFlags && other->m_hints == m_hints;
  }


  bool DxvkMemoryChunk::checkHints(DxvkMemoryFlags hints) const {
    DxvkMemoryFlags mask(
      DxvkMemoryFlag::Small,
      DxvkMemoryFlag::GpuReadable,
      DxvkMemoryFlag::GpuWritable,
      DxvkMemoryFlag::Transient);

    if (hints.test(DxvkMemoryFlag::IgnoreConstraints))
      mask = DxvkMemoryFlags();

    return (m_hints & mask) == (hints & mask);
  }


  DxvkMemoryAllocator::DxvkMemoryAllocator(const DxvkDevice* device)
  : m_vkd             (device->vkd()),
    m_device          (device),
    m_devProps        (device->adapter()->deviceProperties()),
    m_memProps        (device->adapter()->memoryProperties()) {
    for (uint32_t i = 0; i < m_memProps.memoryHeapCount; i++) {
      m_memHeaps[i].properties = m_memProps.memoryHeaps[i];
      m_memHeaps[i].stats      = DxvkMemoryStats { 0, 0 };
      m_memHeaps[i].budget     = 0;

      /* Target 80% of a heap on systems where we want
       * to avoid oversubscribing memory heaps */
      if ((m_memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
       && (m_device->isUnifiedMemoryArchitecture()))
        m_memHeaps[i].budget = (8 * m_memProps.memoryHeaps[i].size) / 10;
    }
    
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++) {
      m_memTypes[i].heap       = &m_memHeaps[m_memProps.memoryTypes[i].heapIndex];
      m_memTypes[i].heapId     = m_memProps.memoryTypes[i].heapIndex;
      m_memTypes[i].memType    = m_memProps.memoryTypes[i];
      m_memTypes[i].memTypeId  = i;
    }
  }
  
  
  DxvkMemoryAllocator::~DxvkMemoryAllocator() {
    
  }
  
  
  DxvkMemory DxvkMemoryAllocator::alloc(
    const VkMemoryRequirements*             req,
    const VkMemoryDedicatedRequirements&    dedAllocReq,
    const VkMemoryDedicatedAllocateInfo&    dedAllocInfo,
          VkMemoryPropertyFlags             flags,
          DxvkMemoryFlags                   hints) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    // Keep small allocations together to avoid fragmenting
    // chunks for larger resources with lots of small gaps,
    // as well as resources with potentially weird lifetimes
    if (req->size <= SmallAllocationThreshold) {
      hints.set(DxvkMemoryFlag::Small);
      hints.clr(DxvkMemoryFlag::GpuWritable, DxvkMemoryFlag::GpuReadable);
    }

    // Ignore most hints for host-visible allocations since they
    // usually don't make much sense for those resources
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      hints = hints & DxvkMemoryFlag::Transient;

    // Try to allocate from a memory type which supports the given flags exactly
    auto dedAllocPtr = dedAllocReq.prefersDedicatedAllocation ? &dedAllocInfo : nullptr;
    DxvkMemory result = this->tryAlloc(req, dedAllocPtr, flags, hints);

    // If the first attempt failed, try ignoring the dedicated allocation
    if (!result && dedAllocPtr && !dedAllocReq.requiresDedicatedAllocation) {
      result = this->tryAlloc(req, nullptr, flags, hints);
      dedAllocPtr = nullptr;
    }

    // Retry without the hint constraints
    if (!result) {
      hints.set(DxvkMemoryFlag::IgnoreConstraints);
      result = this->tryAlloc(req, nullptr, flags, hints);
    }

    // If that still didn't work, probe slower memory types as well
    VkMemoryPropertyFlags optFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                   | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    VkMemoryPropertyFlags remFlags = 0;
    
    while (!result && (flags & optFlags)) {
      remFlags |= optFlags & -optFlags;
      optFlags &= ~remFlags;

      result = this->tryAlloc(req, dedAllocPtr, flags & ~remFlags, hints);
    }
    
    if (!result) {
      DxvkAdapterMemoryInfo memHeapInfo = m_device->adapter()->getMemoryHeapInfo();

      Logger::err(str::format(
        "DxvkMemoryAllocator: Memory allocation failed",
        "\n  Size:      ", req->size,
        "\n  Alignment: ", req->alignment,
        "\n  Mem flags: ", "0x", std::hex, flags,
        "\n  Mem types: ", "0x", std::hex, req->memoryTypeBits));

      for (uint32_t i = 0; i < m_memProps.memoryHeapCount; i++) {
        Logger::err(str::format("Heap ", i, ": ",
          (m_memHeaps[i].stats.memoryAllocated >> 20), " MB allocated, ",
          (m_memHeaps[i].stats.memoryUsed      >> 20), " MB used, ",
          m_device->extensions().extMemoryBudget
            ? str::format(
                (memHeapInfo.heaps[i].memoryAllocated >> 20), " MB allocated (driver), ",
                (memHeapInfo.heaps[i].memoryBudget    >> 20), " MB budget (driver), ",
                (m_memHeaps[i].properties.size        >> 20), " MB total")
            : str::format(
                (m_memHeaps[i].properties.size        >> 20), " MB total")));
      }

      throw DxvkError("DxvkMemoryAllocator: Memory allocation failed");
    }
    
    return result;
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAlloc(
    const VkMemoryRequirements*             req,
    const VkMemoryDedicatedAllocateInfo*    dedAllocInfo,
          VkMemoryPropertyFlags             flags,
          DxvkMemoryFlags                   hints) {
    DxvkMemory result;

    for (uint32_t i = 0; i < m_memProps.memoryTypeCount && !result; i++) {
      const bool supported = (req->memoryTypeBits & (1u << i)) != 0;
      const bool adequate  = (m_memTypes[i].memType.propertyFlags & flags) == flags;
      
      if (supported && adequate) {
        result = this->tryAllocFromType(&m_memTypes[i],
          flags, req->size, req->alignment, hints, dedAllocInfo);
      }
    }
    
    return result;
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAllocFromType(
          DxvkMemoryType*                   type,
          VkMemoryPropertyFlags             flags,
          VkDeviceSize                      size,
          VkDeviceSize                      align,
          DxvkMemoryFlags                   hints,
    const VkMemoryDedicatedAllocateInfo*    dedAllocInfo) {
    VkDeviceSize chunkSize = pickChunkSize(type->memTypeId, hints);

    DxvkMemory memory;

    if (size >= chunkSize || dedAllocInfo) {
      if (this->shouldFreeEmptyChunks(type->heap, size))
        this->freeEmptyChunks(type->heap);

      DxvkDeviceMemory devMem = this->tryAllocDeviceMemory(
        type, flags, size, hints, dedAllocInfo);

      if (devMem.memHandle != VK_NULL_HANDLE)
        memory = DxvkMemory(this, nullptr, type, devMem.memHandle, 0, size, devMem.memPointer);
    } else {
      for (uint32_t i = 0; i < type->chunks.size() && !memory; i++)
        memory = type->chunks[i]->alloc(flags, size, align, hints);
      
      if (!memory) {
        DxvkDeviceMemory devMem;
        
        if (this->shouldFreeEmptyChunks(type->heap, chunkSize))
          this->freeEmptyChunks(type->heap);

        for (uint32_t i = 0; i < 6 && (chunkSize >> i) >= size && !devMem.memHandle; i++)
          devMem = tryAllocDeviceMemory(type, flags, chunkSize >> i, hints, nullptr);

        if (devMem.memHandle) {
          Rc<DxvkMemoryChunk> chunk = new DxvkMemoryChunk(this, type, devMem, hints);
          memory = chunk->alloc(flags, size, align, hints);

          type->chunks.push_back(std::move(chunk));
        }
      }
    }

    if (memory)
      type->heap->stats.memoryUsed += memory.m_length;

    return memory;
  }
  
  
  DxvkDeviceMemory DxvkMemoryAllocator::tryAllocDeviceMemory(
          DxvkMemoryType*                   type,
          VkMemoryPropertyFlags             flags,
          VkDeviceSize                      size,
          DxvkMemoryFlags                   hints,
    const VkMemoryDedicatedAllocateInfo*    dedAllocInfo) {
    bool useMemoryPriority = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                          && (m_device->features().extMemoryPriority.memoryPriority);
    
    if (type->heap->budget && type->heap->stats.memoryAllocated + size > type->heap->budget)
      return DxvkDeviceMemory();

    float priority = 0.0f;

    if (hints.test(DxvkMemoryFlag::GpuReadable))
      priority = 0.5f;
    if (hints.test(DxvkMemoryFlag::GpuWritable))
      priority = 1.0f;

    DxvkDeviceMemory result;
    result.memSize  = size;
    result.memFlags = flags;
    result.priority = priority;

    VkMemoryPriorityAllocateInfoEXT prio = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
    prio.priority         = priority;

    VkMemoryAllocateInfo info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, dedAllocInfo };
    info.allocationSize   = size;
    info.memoryTypeIndex  = type->memTypeId;

    if (useMemoryPriority)
      prio.pNext = std::exchange(info.pNext, &prio);

    if (m_vkd->vkAllocateMemory(m_vkd->device(), &info, nullptr, &result.memHandle) != VK_SUCCESS)
      return DxvkDeviceMemory();
    
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VkResult status = m_vkd->vkMapMemory(m_vkd->device(), result.memHandle, 0, VK_WHOLE_SIZE, 0, &result.memPointer);

      if (status != VK_SUCCESS) {
        Logger::err(str::format("DxvkMemoryAllocator: Mapping memory failed with ", status));
        m_vkd->vkFreeMemory(m_vkd->device(), result.memHandle, nullptr);
        return DxvkDeviceMemory();
      }
    }

    type->heap->stats.memoryAllocated += size;
    m_device->adapter()->notifyHeapMemoryAlloc(type->heapId, size);
    return result;
  }


  void DxvkMemoryAllocator::free(
    const DxvkMemory&           memory) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    memory.m_type->heap->stats.memoryUsed -= memory.m_length;

    if (memory.m_chunk != nullptr) {
      this->freeChunkMemory(
        memory.m_type,
        memory.m_chunk,
        memory.m_offset,
        memory.m_length);
    } else {
      DxvkDeviceMemory devMem;
      devMem.memHandle  = memory.m_memory;
      devMem.memPointer = nullptr;
      devMem.memSize    = memory.m_length;
      this->freeDeviceMemory(memory.m_type, devMem);
    }
  }

  
  void DxvkMemoryAllocator::freeChunkMemory(
          DxvkMemoryType*       type,
          DxvkMemoryChunk*      chunk,
          VkDeviceSize          offset,
          VkDeviceSize          length) {
    chunk->free(offset, length);

    if (chunk->isEmpty()) {
      Rc<DxvkMemoryChunk> chunkRef = chunk;

      // Free the chunk if we have to, or at least put it at the end of
      // the list so that chunks that are already in use and cannot be
      // freed are prioritized for allocations to reduce memory pressure.
      type->chunks.erase(std::remove(type->chunks.begin(), type->chunks.end(), chunkRef));

      if (!this->shouldFreeChunk(type, chunkRef))
        type->chunks.push_back(std::move(chunkRef));
    }
  }
  

  void DxvkMemoryAllocator::freeDeviceMemory(
          DxvkMemoryType*       type,
          DxvkDeviceMemory      memory) {
    m_vkd->vkFreeMemory(m_vkd->device(), memory.memHandle, nullptr);
    type->heap->stats.memoryAllocated -= memory.memSize;
    m_device->adapter()->notifyHeapMemoryFree(type->heapId, memory.memSize);
  }


  VkDeviceSize DxvkMemoryAllocator::pickChunkSize(uint32_t memTypeId, DxvkMemoryFlags hints) const {
    VkMemoryType type = m_memProps.memoryTypes[memTypeId];
    VkMemoryHeap heap = m_memProps.memoryHeaps[type.heapIndex];

    // Default to a chunk size of 128 MiB
    VkDeviceSize chunkSize = 128 << 20;

    if (hints.test(DxvkMemoryFlag::Small))
      chunkSize = 16 << 20;

    // Try to waste a bit less system memory especially in
    // 32-bit applications due to address space constraints
    if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      chunkSize = (env::is32BitHostPlatform() ? 16 : 64) << 20;

    // Reduce the chunk size on small heaps so
    // we can at least fit in 15 allocations
    while (chunkSize * 15 > heap.size)
      chunkSize >>= 1;

    return chunkSize;
  }


  bool DxvkMemoryAllocator::shouldFreeChunk(
    const DxvkMemoryType*       type,
    const Rc<DxvkMemoryChunk>&  chunk) const {
    // Under memory pressure, we should start freeing everything.
    if (this->shouldFreeEmptyChunks(type->heap, 0))
      return true;

    // Even if we have enough memory to spare, only keep
    // one chunk of each type around to save memory.
    for (const auto& c : type->chunks) {
      if (c != chunk && c->isEmpty() && c->isCompatible(chunk))
        return true;
    }

    return false;
  }


  bool DxvkMemoryAllocator::shouldFreeEmptyChunks(
    const DxvkMemoryHeap*       heap,
          VkDeviceSize          allocationSize) const {
    VkDeviceSize budget = heap->budget;

    if (!budget)
      budget = (heap->properties.size * 4) / 5;

    return heap->stats.memoryAllocated + allocationSize > budget;
  }


  void DxvkMemoryAllocator::freeEmptyChunks(
    const DxvkMemoryHeap*       heap) {
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++) {
      DxvkMemoryType* type = &m_memTypes[i];

      if (type->heap != heap)
        continue;

      type->chunks.erase(
        std::remove_if(type->chunks.begin(), type->chunks.end(),
          [] (const Rc<DxvkMemoryChunk>& chunk) { return chunk->isEmpty(); }),
        type->chunks.end());
    }
  }

}
