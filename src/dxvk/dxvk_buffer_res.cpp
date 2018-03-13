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
    
    if (m_vkd->vkCreateBufferView(m_vkd->device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS)
      throw DxvkError("DxvkBufferView::DxvkBufferView: Failed to create buffer view");
  }
  
  
  DxvkPhysicalBufferView::~DxvkPhysicalBufferView() {
    m_vkd->vkDestroyBufferView(
      m_vkd->device(), m_view, nullptr);
  }
  
}