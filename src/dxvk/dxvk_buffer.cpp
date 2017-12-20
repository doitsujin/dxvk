#include "dxvk_buffer.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkBufferResource::DxvkBufferResource(
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
  
  
  DxvkBufferResource::~DxvkBufferResource() {
    if (m_buffer != VK_NULL_HANDLE)
      m_vkd->vkDestroyBuffer(m_vkd->device(), m_buffer, nullptr);
  }
  
  
  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType)
  : m_device  (device),
    m_info    (createInfo),
    m_memFlags(memoryType),
    m_resource(allocateResource()) {
    
  }
  
  
  void DxvkBuffer::renameResource(
    const Rc<DxvkBufferResource>& resource) {
    m_resource = resource;
  }
  
  
  Rc<DxvkBufferResource> DxvkBuffer::allocateResource() {
    return m_device->allocBufferResource(m_info, m_memFlags);
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