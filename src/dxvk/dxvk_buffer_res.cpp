#include "dxvk_buffer_res.h"

namespace dxvk {
  
  DxvkPhysicalBuffer::DxvkPhysicalBuffer(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkBufferCreateInfo& createInfo,
          VkDeviceSize          sliceCount,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd         (vkd),
    m_sliceCount  (sliceCount),
    m_sliceLength (createInfo.size),
    m_sliceStride (align(createInfo.size, 256)) {
    
    VkBufferCreateInfo info;
    info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext                 = nullptr;
    info.flags                 = 0;
    info.size                  = m_sliceStride * sliceCount;
    info.usage                 = createInfo.usage;
    info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices   = nullptr;
    
    if (m_vkd->vkCreateBuffer(m_vkd->device(),
          &info, nullptr, &m_handle) != VK_SUCCESS)
      throw DxvkError("DxvkPhysicalBuffer: Failed to create buffer");
    
    VkMemoryRequirements memReq;
    m_vkd->vkGetBufferMemoryRequirements(
      m_vkd->device(), m_handle, &memReq);
    m_memory = memAlloc.alloc(memReq, memFlags);
    
    if (m_vkd->vkBindBufferMemory(m_vkd->device(),
          m_handle, m_memory.memory(), m_memory.offset()) != VK_SUCCESS)
      throw DxvkError("DxvkPhysicalBuffer: Failed to bind device memory");
  }
  
  
  DxvkPhysicalBuffer::~DxvkPhysicalBuffer() {
    if (m_handle != VK_NULL_HANDLE)
      m_vkd->vkDestroyBuffer(m_vkd->device(), m_handle, nullptr);
  }
  
  
  DxvkPhysicalBufferSlice DxvkPhysicalBuffer::slice(uint32_t id) {
    return DxvkPhysicalBufferSlice(this, id * m_sliceStride, m_sliceLength);
  }
  
}