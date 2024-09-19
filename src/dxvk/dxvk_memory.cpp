#include <algorithm>
#include <iomanip>
#include <sstream>

#include "dxvk_device.h"
#include "dxvk_memory.h"

namespace dxvk {
  
  DxvkMemory::DxvkMemory() { }
  DxvkMemory::DxvkMemory(
          DxvkMemoryAllocator*  alloc,
          DxvkMemoryChunk*      chunk,
          DxvkMemoryType*       type,
          VkBuffer              buffer,
          VkDeviceMemory        memory,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          void*                 mapPtr)
  : m_alloc   (alloc),
    m_chunk   (chunk),
    m_type    (type),
    m_buffer  (buffer),
    m_memory  (memory),
    m_offset  (offset),
    m_length  (length),
    m_mapPtr  (mapPtr) { }
  
  
  DxvkMemory::DxvkMemory(DxvkMemory&& other)
  : m_alloc   (std::exchange(other.m_alloc,  nullptr)),
    m_chunk   (std::exchange(other.m_chunk,  nullptr)),
    m_type    (std::exchange(other.m_type,   nullptr)),
    m_buffer  (std::exchange(other.m_buffer, VkBuffer(VK_NULL_HANDLE))),
    m_memory  (std::exchange(other.m_memory, VkDeviceMemory(VK_NULL_HANDLE))),
    m_offset  (std::exchange(other.m_offset, 0)),
    m_length  (std::exchange(other.m_length, 0)),
    m_mapPtr  (std::exchange(other.m_mapPtr, nullptr)) { }
  
  
  DxvkMemory& DxvkMemory::operator = (DxvkMemory&& other) {
    this->free();
    m_alloc   = std::exchange(other.m_alloc,  nullptr);
    m_chunk   = std::exchange(other.m_chunk,  nullptr);
    m_type    = std::exchange(other.m_type,   nullptr);
    m_buffer  = std::exchange(other.m_buffer, VkBuffer(VK_NULL_HANDLE));
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
      m_memory.buffer, m_memory.memHandle, allocStart, allocEnd - allocStart,
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


  DxvkMemoryAllocator::DxvkMemoryAllocator(DxvkDevice* device)
  : m_device          (device),
    m_memProps        (device->adapter()->memoryProperties()) {
    for (uint32_t i = 0; i < m_memProps.memoryHeapCount; i++) {
      m_memHeaps[i].properties = m_memProps.memoryHeaps[i];
      m_memHeaps[i].stats      = DxvkMemoryStats { 0, 0 };
    }
    
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++) {
      m_memTypes[i].heap       = &m_memHeaps[m_memProps.memoryTypes[i].heapIndex];
      m_memTypes[i].heapId     = m_memProps.memoryTypes[i].heapIndex;
      m_memTypes[i].memType    = m_memProps.memoryTypes[i];
      m_memTypes[i].memTypeId  = i;
      m_memTypes[i].chunkSize  = MinChunkSize;
      m_memTypes[i].bufferUsage = 0;
    }

    if (device->features().core.features.sparseBinding)
      m_sparseMemoryTypes = determineSparseMemoryTypes(device);

    determineBufferUsageFlagsPerMemoryType();
  }
  
  
  DxvkMemoryAllocator::~DxvkMemoryAllocator() {
    
  }
  
  
  DxvkMemory DxvkMemoryAllocator::alloc(
          DxvkMemoryRequirements            req,
          DxvkMemoryProperties              info,
          DxvkMemoryFlags                   hints) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    // Keep small allocations together to avoid fragmenting
    // chunks for larger resources with lots of small gaps,
    // as well as resources with potentially weird lifetimes
    if (req.core.memoryRequirements.size <= SmallAllocationThreshold) {
      hints.set(DxvkMemoryFlag::Small);
      hints.clr(DxvkMemoryFlag::GpuWritable, DxvkMemoryFlag::GpuReadable);
    }

    // Ignore most hints for host-visible allocations since they
    // usually don't make much sense for those resources
    if (info.flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      hints = hints & DxvkMemoryFlag::Transient;

    // If requested, try with a dedicated allocation first.
    if (info.dedicated.image || info.dedicated.buffer) {
      DxvkMemory result = this->tryAlloc(req, info, hints);

      if (result)
        return result;
    }

    // If possible, retry without a dedicated allocation
    if (!req.dedicated.requiresDedicatedAllocation) {
      info.dedicated.image = VK_NULL_HANDLE;
      info.dedicated.buffer = VK_NULL_HANDLE;

      // If we're allocating tiled image memory, ensure
      // that it will not overlap with buffer memory.
      if (req.tiling == VK_IMAGE_TILING_OPTIMAL) {
        VkDeviceSize granularity = m_device->properties().core.properties.limits.bufferImageGranularity;
        req.core.memoryRequirements.size      = align(req.core.memoryRequirements.size,       granularity);
        req.core.memoryRequirements.alignment = align(req.core.memoryRequirements.alignment,  granularity);
      }

      DxvkMemory result = this->tryAlloc(req, info, hints);

      if (result)
        return result;

      // Retry without the hint constraints
      hints.set(DxvkMemoryFlag::IgnoreConstraints);
      result = this->tryAlloc(req, info, hints);

      if (result)
        return result;
    }

    // If that still didn't work, probe slower memory types as
    // well, but re-enable restrictions to decrease fragmentation.
    hints.clr(DxvkMemoryFlag::IgnoreConstraints);

    const VkMemoryPropertyFlags optionalFlags =
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    if (info.flags & optionalFlags) {
      info.flags &= ~optionalFlags;

      DxvkMemory result = this->tryAlloc(req, info, hints);

      if (result)
        return result;
    }

    // We weren't able to allocate memory for this resource form any type
    this->logMemoryError(req.core.memoryRequirements);
    this->logMemoryStats();

    throw DxvkError("DxvkMemoryAllocator: Memory allocation failed");
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAlloc(
    const DxvkMemoryRequirements&           req,
    const DxvkMemoryProperties&             info,
          DxvkMemoryFlags                   hints) {
    DxvkMemory result;

    for (uint32_t i = 0; i < m_memProps.memoryTypeCount && !result; i++) {
      const bool supported = (req.core.memoryRequirements.memoryTypeBits & (1u << i)) != 0;
      const bool adequate  = (m_memTypes[i].memType.propertyFlags & info.flags) == info.flags;
      
      if (supported && adequate) {
        result = this->tryAllocFromType(&m_memTypes[i],
          req.core.memoryRequirements.size,
          req.core.memoryRequirements.alignment,
          info, hints);
      }
    }
    
    return result;
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAllocFromType(
          DxvkMemoryType*                   type,
          VkDeviceSize                      size,
          VkDeviceSize                      align,
    const DxvkMemoryProperties&             info,
          DxvkMemoryFlags                   hints) {
    constexpr VkDeviceSize DedicatedAllocationThreshold = 3;

    VkDeviceSize chunkSize = pickChunkSize(type->memTypeId,
      DedicatedAllocationThreshold * size, hints);

    DxvkMemory memory;

    // Require dedicated allocations for resources that use the Vulkan dedicated
    // allocation bits, or are too large to fit into a single full-sized chunk
    bool needsDedicatedAlocation = size >= chunkSize || info.dedicated.buffer || info.dedicated.image;

    // Prefer a dedicated allocation for very large resources in order to
    // reduce fragmentation if a large number of those resources are in use
    bool wantsDedicatedAllocation = DedicatedAllocationThreshold * size > chunkSize;

    // Try to reuse existing memory as much as possible in case the heap is nearly full
    bool heapBudgedExceeded = 5 * type->heap->stats.memoryUsed + size > 4 * type->heap->properties.size;

    if (!needsDedicatedAlocation && (!wantsDedicatedAllocation || heapBudgedExceeded)) {
      // Attempt to suballocate from existing chunks first
      for (uint32_t i = 0; i < type->chunks.size() && !memory; i++)
        memory = type->chunks[i]->alloc(info.flags, size, align, hints);

      // If no existing chunk can accomodate the allocation, and if a dedicated
      // allocation is not preferred, create a new chunk and suballocate from it
      if (!memory && !wantsDedicatedAllocation) {
        DxvkDeviceMemory devMem;

        if (this->shouldFreeEmptyChunks(type->heap, chunkSize))
          this->freeEmptyChunks(type->heap);

        for (uint32_t i = 0; i < 6 && (chunkSize >> i) >= size && !devMem.memHandle; i++)
          devMem = tryAllocDeviceMemory(type, chunkSize >> i, info, hints);

        if (devMem.memHandle) {
          Rc<DxvkMemoryChunk> chunk = new DxvkMemoryChunk(this, type, devMem, hints);
          memory = chunk->alloc(info.flags, size, align, hints);

          type->chunks.push_back(std::move(chunk));

          adjustChunkSize(type->memTypeId, devMem.memSize, hints);
        }
      }
    }

    // If a dedicated allocation is required or preferred and we haven't managed
    // to suballocate any memory before, try to create a dedicated allocation
    if (!memory && (needsDedicatedAlocation || wantsDedicatedAllocation)) {
      if (this->shouldFreeEmptyChunks(type->heap, size))
        this->freeEmptyChunks(type->heap);

      DxvkDeviceMemory devMem = this->tryAllocDeviceMemory(type, size, info, hints);

      if (devMem.memHandle != VK_NULL_HANDLE) {
        memory = DxvkMemory(this, nullptr, type,
          devMem.buffer, devMem.memHandle, 0, size, devMem.memPointer);
      }
    }

    if (memory) {
      type->heap->stats.memoryUsed += memory.m_length;
      m_device->notifyMemoryUse(type->heapId, memory.m_length);
    }

    return memory;
  }
  
  
  DxvkDeviceMemory DxvkMemoryAllocator::tryAllocDeviceMemory(
          DxvkMemoryType*                   type,
          VkDeviceSize                      size,
          DxvkMemoryProperties              info,
          DxvkMemoryFlags                   hints) {
    auto vk = m_device->vkd();

    bool useMemoryPriority = (info.flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                          && (m_device->features().extMemoryPriority.memoryPriority);
    
    float priority = 0.0f;

    if (hints.test(DxvkMemoryFlag::GpuReadable))
      priority = 0.5f;
    if (hints.test(DxvkMemoryFlag::GpuWritable))
      priority = 1.0f;

    bool dedicated = info.dedicated.buffer || info.dedicated.image;

    DxvkDeviceMemory result;
    result.memSize  = size;
    result.memFlags = info.flags;
    result.priority = priority;

    VkMemoryAllocateFlagsInfo memoryFlags = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
    memoryFlags.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
    priorityInfo.priority       = priority;

    VkMemoryAllocateInfo memoryInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memoryInfo.allocationSize   = size;
    memoryInfo.memoryTypeIndex  = type->memTypeId;

    if (info.sharedExport.handleTypes)
      info.sharedExport.pNext = std::exchange(memoryInfo.pNext, &info.sharedExport);

    if (info.sharedImportWin32.handleType)
      info.sharedImportWin32.pNext = std::exchange(memoryInfo.pNext, &info.sharedImportWin32);

    if (info.dedicated.buffer || info.dedicated.image)
      info.dedicated.pNext = std::exchange(memoryInfo.pNext, &info.dedicated);

    if (useMemoryPriority)
      priorityInfo.pNext = std::exchange(memoryInfo.pNext, &priorityInfo);

    if (!info.dedicated.image && (type->bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
      memoryFlags.pNext = std::exchange(memoryInfo.pNext, &memoryFlags);

    if (vk->vkAllocateMemory(vk->device(), &memoryInfo, nullptr, &result.memHandle))
      return DxvkDeviceMemory();
    
    if (info.flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VkResult status = vk->vkMapMemory(vk->device(), result.memHandle, 0, VK_WHOLE_SIZE, 0, &result.memPointer);

      if (status) {
        Logger::err(str::format("DxvkMemoryAllocator: Mapping memory failed with ", status));
        vk->vkFreeMemory(vk->device(), result.memHandle, nullptr);
        return DxvkDeviceMemory();
      }
    }

    if (type->bufferUsage && !dedicated) {
      VkBuffer buffer = VK_NULL_HANDLE;

      VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
      bufferInfo.size = result.memSize;
      bufferInfo.usage = type->bufferUsage;
      bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

      VkResult status = vk->vkCreateBuffer(vk->device(), &bufferInfo, nullptr, &buffer);

      if (status == VK_SUCCESS) {
        VkBufferMemoryRequirementsInfo2 memInfo = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 };
        memInfo.buffer = buffer;

        VkMemoryRequirements2 requirements = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vk->vkGetBufferMemoryRequirements2(vk->device(), &memInfo, &requirements);

        if ((requirements.memoryRequirements.size == size)
         && (requirements.memoryRequirements.memoryTypeBits & (1u << type->memTypeId))) {
          status = vk->vkBindBufferMemory(vk->device(), buffer, result.memHandle, 0);

          if (status == VK_SUCCESS)
            result.buffer = buffer;
        }

        if (!result.buffer)
          vk->vkDestroyBuffer(vk->device(), buffer, nullptr);
      }

      if (!result.buffer) {
        Logger::warn(str::format("Failed to create global buffer:",
          "\n  size:  ", std::dec, size,
          "\n  usage: ", std::hex, type->bufferUsage,
          "\n  type:  ", std::dec, type->memTypeId));
      }
    }

    type->heap->stats.memoryAllocated += size;
    m_device->notifyMemoryAlloc(type->heapId, size);
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
      devMem.buffer     = memory.m_buffer;
      devMem.memHandle  = memory.m_memory;
      devMem.memPointer = nullptr;
      devMem.memSize    = memory.m_length;
      this->freeDeviceMemory(memory.m_type, devMem);
    }

    m_device->notifyMemoryUse(memory.m_type->heapId, -memory.m_length);
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
    auto vk = m_device->vkd();
    vk->vkDestroyBuffer(vk->device(), memory.buffer, nullptr);
    vk->vkFreeMemory(vk->device(), memory.memHandle, nullptr);

    type->heap->stats.memoryAllocated -= memory.memSize;
    m_device->notifyMemoryAlloc(type->heapId, memory.memSize);
  }


  VkDeviceSize DxvkMemoryAllocator::pickChunkSize(uint32_t memTypeId, VkDeviceSize requiredSize, DxvkMemoryFlags hints) const {
    VkMemoryType type = m_memProps.memoryTypes[memTypeId];
    VkMemoryHeap heap = m_memProps.memoryHeaps[type.heapIndex];

    VkDeviceSize chunkSize = m_memTypes[memTypeId].chunkSize;

    while (chunkSize < requiredSize && chunkSize < MaxChunkSize)
      chunkSize <<= 1u;

    if (hints.test(DxvkMemoryFlag::Small))
      chunkSize = std::min<VkDeviceSize>(chunkSize, 16 << 20);

    // Try to waste a bit less system memory especially in
    // 32-bit applications due to address space constraints
    if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      chunkSize = std::min<VkDeviceSize>((env::is32BitHostPlatform() ? 16 : 64) << 20, chunkSize);

    // Reduce the chunk size on small heaps so
    // we can at least fit in 15 allocations
    while (chunkSize * 15 > heap.size)
      chunkSize >>= 1;

    return chunkSize;
  }


  void DxvkMemoryAllocator::adjustChunkSize(
          uint32_t              memTypeId,
          VkDeviceSize          allocatedSize,
          DxvkMemoryFlags       hints) {
    VkDeviceSize chunkSize = m_memTypes[memTypeId].chunkSize;

    // Don't bump chunk size if we reached the maximum or if
    // we already were unable to allocate a full chunk.
    if (chunkSize <= allocatedSize && chunkSize <= m_memTypes[memTypeId].heap->stats.memoryAllocated)
      m_memTypes[memTypeId].chunkSize = pickChunkSize(memTypeId, chunkSize * 2, DxvkMemoryFlags());
  }


  bool DxvkMemoryAllocator::shouldFreeChunk(
    const DxvkMemoryType*       type,
    const Rc<DxvkMemoryChunk>&  chunk) const {
    // Under memory pressure, we should start freeing everything.
    if (this->shouldFreeEmptyChunks(type->heap, 0))
      return true;

    // Free chunks that are below the current chunk size since it probably
    // not going to be able to serve enough allocations to be useful.
    if (chunk->size() < type->chunkSize)
      return true;

    // Only keep a small number of chunks of each type around to save memory.
    uint32_t numEmptyChunks = 0;

    for (const auto& c : type->chunks) {
      if (c != chunk && c->isEmpty() && c->isCompatible(chunk))
        numEmptyChunks += 1;
    }

    // Be a bit more lenient on system memory since data uploads may otherwise
    // lead to a large number of allocations and deallocations at runtime.
    uint32_t maxEmptyChunks = env::is32BitHostPlatform() ? 2 : 4;

    if ((type->memType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
     || !(type->memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
      maxEmptyChunks = 1;

    return numEmptyChunks >= maxEmptyChunks;
  }


  bool DxvkMemoryAllocator::shouldFreeEmptyChunks(
    const DxvkMemoryHeap*       heap,
          VkDeviceSize          allocationSize) const {
    VkDeviceSize budget = (heap->properties.size * 4) / 5;
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
    bufferInfo.sharingMode  = VK_SHARING_MODE_EXCLUSIVE;

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
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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

          if (type < m_memProps.memoryTypeCount)
            m_memTypes.at(type).bufferUsage |= bufferInfo.usage;

          typeMask &= typeMask - 1;
        }
      }

      flags &= ~flag;
    }

    // Only use a minimal set of usage flags for the global buffer if the
    // full combination of flags is not supported for whatever reason.
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++) {
      bufferInfo.usage = m_memTypes[i].bufferUsage;

      if (!getBufferMemoryRequirements(bufferInfo, requirements)
       || !(requirements.memoryRequirements.memoryTypeBits & (1u << i))) {
        m_memTypes[i].bufferUsage &= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                                  |  VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                  |  VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      }
    }
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

    for (uint32_t i = 0; i < m_memProps.memoryHeapCount; i++) {
      sstr << std::setw(2) << i << ":   "
           << std::setw(6) << (m_memHeaps[i].properties.size >> 20) << "      "
           << std::setw(6) << (m_memHeaps[i].stats.memoryAllocated >> 20) << "      "
           << std::setw(6) << (m_memHeaps[i].stats.memoryUsed >> 20) << "      ";

      if (m_device->features().extMemoryBudget) {
        sstr << std::setw(6) << (memHeapInfo.heaps[i].memoryAllocated >> 20) << "      "
             << std::setw(6) << (memHeapInfo.heaps[i].memoryBudget >> 20) << "      " << std::endl;
      } else {
        sstr << " n/a         n/a" << std::endl;
      }
    }

    Logger::err(sstr.str());
  }

}
