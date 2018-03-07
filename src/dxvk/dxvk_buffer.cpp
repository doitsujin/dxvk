#include "dxvk_buffer.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType)
  : m_device        (device),
    m_info          (createInfo),
    m_memFlags      (memoryType) {
    // Align physical buffer slices to 256 bytes, which guarantees
    // that we don't violate any Vulkan alignment requirements
    m_physSliceLength = createInfo.size;
    m_physSliceStride = align(createInfo.size, 256);
    
    // Initialize a single backing bufer with one slice
    m_physBuffers[0] = this->allocPhysicalBuffer(1);
    m_physSlice      = this->allocPhysicalSlice();
  }
  
  
  void DxvkBuffer::rename(const DxvkPhysicalBufferSlice& slice) {
    m_physSlice = slice;
    m_revision += 1;
  }
  
  
  DxvkPhysicalBufferSlice DxvkBuffer::allocPhysicalSlice() {
    if (m_physSliceId >= m_physSliceCount) {
      m_physBufferId = (m_physBufferId + 1) % m_physBuffers.size();
      m_physSliceId  = 0;
      
      if (m_physBuffers[m_physBufferId] == nullptr) {
        // Make sure that all buffers have the same size. If we don't do this,
        // one of the physical buffers may grow indefinitely while the others
        // remain small, depending on the usage pattern of the application.
        m_physBuffers[m_physBufferId] = this->allocPhysicalBuffer(m_physSliceCount);
      } else if (m_physBuffers[m_physBufferId]->isInUse()) {
        // Allocate a new physical buffer if the current one is still in use.
        // This also indicates that the buffer gets updated frequently, so we
        // will double the size of the physical buffers to accomodate for it.
        if (m_physBufferId == 0) {
          std::fill(m_physBuffers.begin(), m_physBuffers.end(), nullptr);
          m_physSliceCount *= 2;
        }
        
        m_physBuffers[m_physBufferId] = this->allocPhysicalBuffer(m_physSliceCount);
      }
    }
    
    return m_physBuffers[m_physBufferId]->slice(
      m_physSliceStride * m_physSliceId++,
      m_physSliceLength);
  }
  
  
  Rc<DxvkPhysicalBuffer> DxvkBuffer::allocPhysicalBuffer(VkDeviceSize sliceCount) const {
    DxvkBufferCreateInfo createInfo = m_info;
    createInfo.size = sliceCount * m_physSliceStride;
    
    return m_device->allocPhysicalBuffer(createInfo, m_memFlags);
  }
  
  
  DxvkBufferView::DxvkBufferView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& info)
  : m_vkd(vkd), m_info(info), m_buffer(buffer),
    m_physView(this->createView()),
    m_revision(m_buffer->m_revision) {
    
  }
  
  
  DxvkBufferView::~DxvkBufferView() {
    
  }
  
  
  void DxvkBufferView::updateView() {
    if (m_revision != m_buffer->m_revision) {
      m_physView = this->createView();
      m_revision = m_buffer->m_revision;
    }
  }
  
  
  Rc<DxvkPhysicalBufferView> DxvkBufferView::createView() {
    return new DxvkPhysicalBufferView(
      m_vkd, m_buffer->slice(), m_info);
  }
  
}