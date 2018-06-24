#include "dxvk_buffer_res.h"

namespace dxvk {
  
  DxvkPhysicalBuffer::DxvkPhysicalBuffer(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkBufferCreateInfo& createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd(vkd) {
    
    VkBufferCreateInfo info;
    info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext                 = nullptr;
    info.flags                 = 0;
    info.size                  = createInfo.size;
    info.usage                 = createInfo.usage;
    info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices   = nullptr;
    
    if (m_vkd->vkCreateBuffer(m_vkd->device(),
          &info, nullptr, &m_handle) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkPhysicalBuffer: Failed to create buffer:"
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
    memReqInfo.buffer = m_handle;
    memReqInfo.pNext  = VK_NULL_HANDLE;
    
    VkMemoryDedicatedAllocateInfoKHR dedMemoryAllocInfo;
    dedMemoryAllocInfo.sType  = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
    dedMemoryAllocInfo.pNext  = VK_NULL_HANDLE;
    dedMemoryAllocInfo.buffer = m_handle;
    dedMemoryAllocInfo.image  = VK_NULL_HANDLE;

    m_vkd->vkGetBufferMemoryRequirements2KHR(
       m_vkd->device(), &memReqInfo, &memReq);

    bool useDedicated = dedicatedRequirements.prefersDedicatedAllocation;
    m_memory = memAlloc.alloc(&memReq.memoryRequirements,
      useDedicated ? &dedMemoryAllocInfo : nullptr, memFlags);
    
    if (m_vkd->vkBindBufferMemory(m_vkd->device(), m_handle,
        m_memory.memory(), m_memory.offset()) != VK_SUCCESS)
      throw DxvkError("DxvkPhysicalBuffer: Failed to bind device memory");
  }
  
  
  DxvkPhysicalBuffer::~DxvkPhysicalBuffer() {
    if (m_handle != VK_NULL_HANDLE)
      m_vkd->vkDestroyBuffer(m_vkd->device(), m_handle, nullptr);
  }
  
  
  DxvkPhysicalBufferView::DxvkPhysicalBufferView(
    const Rc<vk::DeviceFn>&         vkd,
    const DxvkPhysicalBufferSlice&  slice,
    const DxvkBufferViewCreateInfo& info)
  : m_vkd(vkd), m_slice(slice.subSlice(info.rangeOffset, info.rangeLength)) {
    VkBufferViewCreateInfo viewInfo;
    viewInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    viewInfo.pNext  = nullptr;
    viewInfo.flags  = 0;
    viewInfo.buffer = m_slice.handle();
    viewInfo.format = info.format;
    viewInfo.offset = m_slice.offset();
    viewInfo.range  = m_slice.length();
    
    if (m_vkd->vkCreateBufferView(m_vkd->device(),
          &viewInfo, nullptr, &m_view) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkPhysicalBufferView: Failed to create buffer view:",
        "\n  Offset: ", viewInfo.offset,
        "\n  Range:  ", viewInfo.range,
        "\n  Format: ", viewInfo.format));
    }
  }
  
  
  DxvkPhysicalBufferView::~DxvkPhysicalBufferView() {
    m_vkd->vkDestroyBufferView(
      m_vkd->device(), m_view, nullptr);
  }
  
}