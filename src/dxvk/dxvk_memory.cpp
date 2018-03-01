#include "dxvk_memory.h"

namespace dxvk {
  
  DxvkMemory::DxvkMemory() {
    
  }
  
  
  DxvkMemory::DxvkMemory(
          DxvkMemoryChunk*  chunk,
          DxvkMemoryHeap*   heap,
          VkDeviceMemory    memory,
          VkDeviceSize      offset,
          VkDeviceSize      length,
          void*             mapPtr)
  : m_chunk   (chunk),
    m_heap    (heap),
    m_memory  (memory),
    m_offset  (offset),
    m_length  (length),
    m_mapPtr  (mapPtr) { }
  
  
  DxvkMemory::DxvkMemory(DxvkMemory&& other)
  : m_chunk   (std::exchange(other.m_chunk,  nullptr)),
    m_heap    (std::exchange(other.m_heap,   nullptr)),
    m_memory  (std::exchange(other.m_memory, VkDeviceMemory(nullptr))),
    m_offset  (std::exchange(other.m_offset, 0)),
    m_length  (std::exchange(other.m_length, 0)),
    m_mapPtr  (std::exchange(other.m_mapPtr, nullptr)) { }
  
  
  DxvkMemory& DxvkMemory::operator = (DxvkMemory&& other) {
    m_chunk   = std::exchange(other.m_chunk,  nullptr);
    m_heap    = std::exchange(other.m_heap,   nullptr);
    m_memory  = std::exchange(other.m_memory, VkDeviceMemory(nullptr));
    m_offset  = std::exchange(other.m_offset, 0);
    m_length  = std::exchange(other.m_length, 0);
    m_mapPtr  = std::exchange(other.m_mapPtr, nullptr);
    return *this;
  }
  
  
  DxvkMemory::~DxvkMemory() {
    if (m_chunk != nullptr)
      m_heap->free(m_chunk, m_offset, m_length);
    else if (m_heap != nullptr)
      m_heap->freeDeviceMemory(m_memory);
  }
  

  DxvkMemoryChunk::DxvkMemoryChunk(
          DxvkMemoryHeap* heap,
          VkDeviceMemory  memory,
          void*           mapPtr,
          VkDeviceSize    size)
  : m_heap  (heap),
    m_memory(memory),
    m_mapPtr(mapPtr),
    m_size  (size) {
    // Mark the entire chunk as free
    m_freeList.push_back(FreeSlice { 0, size });
  }
  
  
  DxvkMemoryChunk::~DxvkMemoryChunk() {
    m_heap->freeDeviceMemory(m_memory);
  }
  
  
  DxvkMemory DxvkMemoryChunk::alloc(VkDeviceSize size, VkDeviceSize align) {
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
    m_delta++;
    return DxvkMemory(this, m_heap,
      m_memory, allocStart, allocEnd - allocStart,
      reinterpret_cast<char*>(m_mapPtr) + allocStart);
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
    
    m_delta--;
    m_freeList.push_back({ offset, length });
  }
  
  
  DxvkMemoryHeap::DxvkMemoryHeap(
    const Rc<vk::DeviceFn>    vkd,
          uint32_t            memTypeId,
          VkMemoryType        memType)
  : m_vkd       (vkd),
    m_memTypeId (memTypeId),
    m_memType   (memType) {
    
  }
  
  
  DxvkMemoryHeap::~DxvkMemoryHeap() {
    
  }
  
  
  DxvkMemory DxvkMemoryHeap::alloc(VkDeviceSize size, VkDeviceSize align) {
    // We don't sub-allocate large allocations from one of the
    // chunks since that might lead to severe fragmentation.
    if (size >= (m_chunkSize / 4)) {
      VkDeviceMemory memory = this->allocDeviceMemory(size);
      
      if (memory == VK_NULL_HANDLE)
        return DxvkMemory();
      
      return DxvkMemory(nullptr, this, memory,
        0, size, this->mapDeviceMemory(memory));
    } else {
      std::lock_guard<std::mutex> lock(m_mutex);
      
      // Probe chunks in a first-fit manner
      for (const auto& chunk : m_chunks) {
        DxvkMemory memory = chunk->alloc(size, align);
        
        if (memory.memory() != VK_NULL_HANDLE)
          return memory;
      }
      
      // None of the existing chunks could satisfy
      // the request, we need to create a new one
      VkDeviceMemory chunkMem = this->allocDeviceMemory(m_chunkSize);
      
      if (chunkMem == VK_NULL_HANDLE)
        return DxvkMemory();
      
      Rc<DxvkMemoryChunk> newChunk = new DxvkMemoryChunk(this,
        chunkMem, this->mapDeviceMemory(chunkMem), m_chunkSize);
      DxvkMemory memory = newChunk->alloc(size, align);
      
      m_chunks.push_back(std::move(newChunk));
      return memory;
    }
  }
  
  
  VkDeviceMemory DxvkMemoryHeap::allocDeviceMemory(VkDeviceSize memorySize) {
    VkMemoryAllocateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext            = nullptr;
    info.allocationSize   = memorySize;
    info.memoryTypeIndex  = m_memTypeId;
    
    VkDeviceMemory memory = VK_NULL_HANDLE;
    
    if (m_vkd->vkAllocateMemory(m_vkd->device(),
        &info, nullptr, &memory) != VK_SUCCESS)
      return VK_NULL_HANDLE;
    
    return memory;
  }
  
  
  void DxvkMemoryHeap::freeDeviceMemory(VkDeviceMemory memory) {
    m_vkd->vkFreeMemory(m_vkd->device(), memory, nullptr);
  }
  
  
  void* DxvkMemoryHeap::mapDeviceMemory(VkDeviceMemory memory) {
    if ((m_memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
      return nullptr;
    
    void* ptr = nullptr;
    
    VkResult status = m_vkd->vkMapMemory(m_vkd->device(),
      memory, 0, VK_WHOLE_SIZE, 0, &ptr);
    
    if (status != VK_SUCCESS) {
      Logger::err("DxvkMemoryHeap: Failed to map memory");
      return nullptr;
    } return ptr;
  }
  
  
  void DxvkMemoryHeap::free(
          DxvkMemoryChunk*  chunk,
          VkDeviceSize      offset,
          VkDeviceSize      length) {
    std::lock_guard<std::mutex> lock(m_mutex);
    chunk->free(offset, length);
  }
  
  
  DxvkMemoryAllocator::DxvkMemoryAllocator(
    const Rc<DxvkAdapter>&  adapter,
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd), m_memProps(adapter->memoryProperties()) {
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++)
      m_heaps[i] = new DxvkMemoryHeap(m_vkd, i, m_memProps.memoryTypes[i]);
  }
  
  
  DxvkMemoryAllocator::~DxvkMemoryAllocator() {
    
  }
  
  
  DxvkMemory DxvkMemoryAllocator::alloc(
    const VkMemoryRequirements& req,
    const VkMemoryPropertyFlags flags) {
    DxvkMemory result = this->tryAlloc(req, flags);
    
    if ((result.memory() == VK_NULL_HANDLE) && (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      result = this->tryAlloc(req, flags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (result.memory() == VK_NULL_HANDLE) {
      throw DxvkError(str::format(
        "DxvkMemoryAllocator: Failed to allocate ",
        req.size, " bytes"));
    }
    
    return result;
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAlloc(
    const VkMemoryRequirements& req,
    const VkMemoryPropertyFlags flags) {
    DxvkMemory result;
    
    for (uint32_t i = 0; i < m_heaps.size() && result.memory() == VK_NULL_HANDLE; i++) {
      const bool supported = (req.memoryTypeBits & (1u << i)) != 0;
      const bool adequate  = (m_memProps.memoryTypes[i].propertyFlags & flags) == flags;
      
      if (supported && adequate)
        result = m_heaps[i]->alloc(req.size, req.alignment);
    }
    
    return result;
  }
  
}