#include "dxvk_memory.h"

namespace dxvk {
  
  DxvkMemory::DxvkMemory() {
    
  }
  
  
  DxvkMemory::DxvkMemory(
    DxvkMemoryAllocator*  alloc,
    VkDeviceMemory        memory,
    void*                 mapPtr)
  : m_alloc (alloc),
    m_memory(memory),
    m_mapPtr(mapPtr) { }
  
  
  DxvkMemory::DxvkMemory(DxvkMemory&& other)
  : m_alloc (other.m_alloc),
    m_memory(other.m_memory),
    m_mapPtr(other.m_mapPtr) {
    other.m_alloc  = nullptr;
    other.m_memory = VK_NULL_HANDLE;
    other.m_mapPtr = nullptr;
  }
  
  
  DxvkMemory& DxvkMemory::operator = (DxvkMemory&& other) {
    this->m_alloc  = other.m_alloc;
    this->m_memory = other.m_memory;
    this->m_mapPtr = other.m_mapPtr;
    other.m_alloc  = nullptr;
    other.m_memory = VK_NULL_HANDLE;
    other.m_mapPtr = nullptr;
    return *this;
  }
  
  
  DxvkMemory::~DxvkMemory() {
    if (m_memory != VK_NULL_HANDLE)
      m_alloc->freeMemory(m_memory);
  }
  
  
  DxvkMemoryAllocator::DxvkMemoryAllocator(
    const Rc<DxvkAdapter>&  adapter,
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd), m_memProps(adapter->memoryProperties()) {
    
  }
  
  
  DxvkMemoryAllocator::~DxvkMemoryAllocator() {
    
  }
  
  
  DxvkMemory DxvkMemoryAllocator::alloc(
    const VkMemoryRequirements& req,
    const VkMemoryPropertyFlags flags) {
    
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void*          mapPtr = nullptr;
    
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++) {
      const bool supported = (req.memoryTypeBits & (1u << i)) != 0;
      const bool adequate  = (m_memProps.memoryTypes[i].propertyFlags & flags) == flags;
      
      if (supported && adequate) {
        memory = this->allocMemory(req.size, i);
        
        if (memory != VK_NULL_HANDLE)
          break;
      }
    }
    
    if (memory == VK_NULL_HANDLE)
      throw DxvkError("DxvkMemoryAllocator::alloc: Failed to allocate memory");
    
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      if (m_vkd->vkMapMemory(m_vkd->device(), memory,
          0, VK_WHOLE_SIZE, 0, &mapPtr) != VK_SUCCESS)
        throw DxvkError("DxvkMemoryAllocator::alloc: Failed to map memory");
    }
    
    return DxvkMemory(this, memory, mapPtr);
  }
  
  
  VkDeviceMemory DxvkMemoryAllocator::allocMemory(
    VkDeviceSize blockSize, uint32_t memoryType) {
    VkMemoryAllocateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext            = nullptr;
    info.allocationSize   = blockSize;
    info.memoryTypeIndex  = memoryType;
    
    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (m_vkd->vkAllocateMemory(m_vkd->device(),
        &info, nullptr, &memory) != VK_SUCCESS)
      return VK_NULL_HANDLE;
    return memory;
  }
  
  
  void DxvkMemoryAllocator::freeMemory(VkDeviceMemory memory) {
    m_vkd->vkFreeMemory(m_vkd->device(), memory, nullptr);
  }
  
}