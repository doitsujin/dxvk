#include "dxvk_buffer.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_device        (device),
    m_info          (createInfo),
    m_memAlloc      (&memAlloc),
    m_memFlags      (memFlags) {
    // Align slices to 256 bytes, which guarantees that
    // we don't violate any Vulkan alignment requirements
    m_physSliceLength = createInfo.size;
    m_physSliceStride = align(createInfo.size, 256);
    
    // Allocate a single buffer slice
    m_buffer = allocBuffer(1);

    m_physSlice.handle = m_buffer.buffer;
    m_physSlice.offset = 0;
    m_physSlice.length = m_physSliceLength;
    m_physSlice.mapPtr = m_buffer.memory.mapPtr(0);
  }


  DxvkBuffer::~DxvkBuffer() {
    auto vkd = m_device->vkd();

    for (const auto& buffer : m_buffers)
      vkd->vkDestroyBuffer(vkd->device(), buffer.buffer, nullptr);
    vkd->vkDestroyBuffer(vkd->device(), m_buffer.buffer, nullptr);
  }
  
  
  DxvkBufferSliceHandle DxvkBuffer::allocSlice() {
    std::unique_lock<sync::Spinlock> freeLock(m_freeMutex);
    
    // If no slices are available, swap the two free lists.
    if (m_freeSlices.size() == 0) {
      std::unique_lock<sync::Spinlock> swapLock(m_swapMutex);
      std::swap(m_freeSlices, m_nextSlices);
    }

    // If there are still no slices available, create a new
    // backing buffer and add all slices to the free list.
    if (m_freeSlices.size() == 0) {
      std::unique_lock<sync::Spinlock> swapLock(m_swapMutex);
      DxvkBufferHandle handle = allocBuffer(m_physSliceCount);
      
      for (uint32_t i = 0; i < m_physSliceCount; i++) {
        DxvkBufferSliceHandle slice;
        slice.handle = handle.buffer;
        slice.offset = m_physSliceStride * i;
        slice.length = m_physSliceLength;
        slice.mapPtr = handle.memory.mapPtr(slice.offset);
        m_freeSlices.push_back(slice);
      }
      
      m_buffers.push_back(std::move(handle));
      m_physSliceCount *= 2;
    }
    
    // Take the first slice from the queue
    DxvkBufferSliceHandle result = std::move(m_freeSlices.back());
    m_freeSlices.pop_back();
    return result;
  }
  
  
  void DxvkBuffer::freeSlice(const DxvkBufferSliceHandle& slice) {
    // Add slice to a separate free list to reduce lock contention.
    std::unique_lock<sync::Spinlock> swapLock(m_swapMutex);
    m_nextSlices.push_back(slice);
  }
  
  
  DxvkBufferHandle DxvkBuffer::allocBuffer(VkDeviceSize sliceCount) const {
    auto vkd = m_device->vkd();

    VkBufferCreateInfo info;
    info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext                 = nullptr;
    info.flags                 = 0;
    info.size                  = m_physSliceStride * sliceCount;
    info.usage                 = m_info.usage;
    info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices   = nullptr;
    
    DxvkBufferHandle handle;

    if (vkd->vkCreateBuffer(vkd->device(),
          &info, nullptr, &handle.buffer) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkBuffer: Failed to create buffer:"
        "\n  size:  ", info.size,
        "\n  usage: ", info.usage));
    }
    
    VkMemoryDedicatedRequirementsKHR dedicatedRequirements;
    dedicatedRequirements.sType                       = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;
    dedicatedRequirements.pNext                       = VK_NULL_HANDLE;
    dedicatedRequirements.prefersDedicatedAllocation  = VK_FALSE;
    dedicatedRequirements.requiresDedicatedAllocation = VK_FALSE;
    
    VkMemoryRequirements2KHR memReq;
    memReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
    memReq.pNext = &dedicatedRequirements;
    
    VkBufferMemoryRequirementsInfo2KHR memReqInfo;
    memReqInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR;
    memReqInfo.buffer = handle.buffer;
    memReqInfo.pNext  = VK_NULL_HANDLE;
    
    VkMemoryDedicatedAllocateInfoKHR dedMemoryAllocInfo;
    dedMemoryAllocInfo.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
    dedMemoryAllocInfo.pNext  = VK_NULL_HANDLE;
    dedMemoryAllocInfo.buffer = handle.buffer;
    dedMemoryAllocInfo.image  = VK_NULL_HANDLE;

    vkd->vkGetBufferMemoryRequirements2KHR(
       vkd->device(), &memReqInfo, &memReq);

    bool useDedicated = dedicatedRequirements.prefersDedicatedAllocation;
    handle.memory = m_memAlloc->alloc(&memReq.memoryRequirements,
      useDedicated ? &dedMemoryAllocInfo : nullptr, m_memFlags);
    
    if (vkd->vkBindBufferMemory(vkd->device(), handle.buffer,
        handle.memory.memory(), handle.memory.offset()) != VK_SUCCESS)
      throw DxvkError("DxvkBuffer: Failed to bind device memory");
    
    return handle;
  }


  
  DxvkBufferView::DxvkBufferView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& info)
  : m_vkd(vkd), m_info(info), m_buffer(buffer),
    m_bufferSlice (m_buffer->getSliceHandle()),
    m_bufferView  (createBufferView(m_bufferSlice)) {
    
  }
  
  
  DxvkBufferView::~DxvkBufferView() {
    if (m_views.empty()) {
      m_vkd->vkDestroyBufferView(
        m_vkd->device(), m_bufferView, nullptr);
    } else {
      for (const auto& pair : m_views) {
        m_vkd->vkDestroyBufferView(
          m_vkd->device(), pair.second, nullptr);
      }
    }
  }
  
  
  VkBufferView DxvkBufferView::createBufferView(
    const DxvkBufferSliceHandle& slice) {
    VkBufferViewCreateInfo viewInfo;
    viewInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    viewInfo.pNext  = nullptr;
    viewInfo.flags  = 0;
    viewInfo.buffer = slice.handle;
    viewInfo.format = m_info.format;
    viewInfo.offset = slice.offset;
    viewInfo.range  = slice.length;
    
    VkBufferView result = VK_NULL_HANDLE;

    if (m_vkd->vkCreateBufferView(m_vkd->device(),
          &viewInfo, nullptr, &result) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkBufferView: Failed to create buffer view:",
        "\n  Offset: ", viewInfo.offset,
        "\n  Range:  ", viewInfo.range,
        "\n  Format: ", viewInfo.format));
    }

    return result;
  }


  void DxvkBufferView::updateBufferView() {
    if (m_views.empty())
      m_views.insert({ m_bufferSlice, m_bufferView });
    
    m_bufferSlice = m_buffer->getSliceHandle();
    auto entry = m_views.find(m_bufferSlice);

    if (entry != m_views.end()) {
      m_bufferView = entry->second;
    } else {
      m_bufferView = createBufferView(m_bufferSlice);
      m_views.insert({ m_bufferSlice, m_bufferView });
    }
  }
  
  
  DxvkBufferTracker:: DxvkBufferTracker() { }
  DxvkBufferTracker::~DxvkBufferTracker() { }
  
  
  void DxvkBufferTracker::freeBufferSlice(
    const Rc<DxvkBuffer>&         buffer,
    const DxvkBufferSliceHandle&  slice) {
    m_entries.push_back({ buffer, slice });
  }
  
  
  void DxvkBufferTracker::reset() {
    for (const auto& e : m_entries)
      e.buffer->freeSlice(e.slice);
      
    m_entries.clear();
  }
  
}