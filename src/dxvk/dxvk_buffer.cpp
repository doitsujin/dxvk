#include "dxvk_buffer.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType)
  : m_device  (device),
    m_info    (createInfo),
    m_memFlags(memoryType),
    m_resource(allocateResource()) {
    
  }
  
  
  void DxvkBuffer::renameResource(const DxvkPhysicalBufferSlice& resource) {
    m_resource = resource;
  }
  
  
  DxvkPhysicalBufferSlice DxvkBuffer::allocateResource() {
    return m_device->allocBufferResource(m_info, m_memFlags)->slice(0);
  }
  
  
  DxvkBufferView::DxvkBufferView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& info)
  : m_vkd(vkd), m_buffer(buffer), m_info(info) {
    this->createBufferView();
  }
  
  
  DxvkBufferView::~DxvkBufferView() {
    this->destroyBufferView();
  }
  
  
  void DxvkBufferView::createBufferView() {
    auto physicalSlice = this->slice();
    
    VkBufferViewCreateInfo viewInfo;
    viewInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    viewInfo.pNext  = nullptr;
    viewInfo.flags  = 0;
    viewInfo.buffer = physicalSlice.handle();
    viewInfo.format = m_info.format;
    viewInfo.offset = physicalSlice.offset();
    viewInfo.range  = physicalSlice.length();
    
    if (m_vkd->vkCreateBufferView(m_vkd->device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS)
      throw DxvkError("DxvkBufferView::DxvkBufferView: Failed to create buffer view");
  }
  
  
  void DxvkBufferView::destroyBufferView() {
    m_vkd->vkDestroyBufferView(
      m_vkd->device(), m_view, nullptr);
  }
  
}