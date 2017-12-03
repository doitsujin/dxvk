#include "dxvk_buffer.h"

namespace dxvk {
  
  DxvkBuffer::DxvkBuffer(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkBufferCreateInfo& createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd(vkd), m_info(createInfo) {
    
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
          &info, nullptr, &m_buffer) != VK_SUCCESS)
      throw DxvkError("DxvkBuffer::DxvkBuffer: Failed to create buffer");
    
    VkMemoryRequirements memReq;
    m_vkd->vkGetBufferMemoryRequirements(
      m_vkd->device(), m_buffer, &memReq);
    m_memory = memAlloc.alloc(memReq, memFlags);
    
    if (m_vkd->vkBindBufferMemory(m_vkd->device(),
          m_buffer, m_memory.memory(), m_memory.offset()) != VK_SUCCESS)
      throw DxvkError("DxvkBuffer::DxvkBuffer: Failed to bind device memory");
  }
  
  
  DxvkBuffer::~DxvkBuffer() {
    if (m_buffer != VK_NULL_HANDLE)
      m_vkd->vkDestroyBuffer(m_vkd->device(), m_buffer, nullptr);
  }
  
  
  DxvkBufferView::DxvkBufferView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& info)
  : m_vkd(vkd), m_buffer(buffer), m_info(info) {
    VkBufferViewCreateInfo viewInfo;
    viewInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    viewInfo.pNext  = nullptr;
    viewInfo.flags  = 0;
    viewInfo.buffer = buffer->handle();
    viewInfo.format = info.format;
    viewInfo.offset = info.rangeOffset;
    viewInfo.range  = info.rangeLength;
    
    if (m_vkd->vkCreateBufferView(m_vkd->device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS)
      throw DxvkError("DxvkBufferView::DxvkBufferView: Failed to create buffer view");
  }
  
  
  DxvkBufferView::~DxvkBufferView() {
    m_vkd->vkDestroyBufferView(
      m_vkd->device(), m_view, nullptr);
  }
  
}