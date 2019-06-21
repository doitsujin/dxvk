#include "dxvk_device.h"
#include "dxvk_staging.h"

namespace dxvk {
  
  DxvkStagingDataAlloc::DxvkStagingDataAlloc(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  DxvkStagingDataAlloc::~DxvkStagingDataAlloc() {

  }


  DxvkBufferSlice DxvkStagingDataAlloc::alloc(VkDeviceSize align, VkDeviceSize size) {
    if (size > MaxBufferSize)
      return DxvkBufferSlice(createBuffer(size));
    
    if (m_buffer == nullptr)
      m_buffer = createBuffer(MaxBufferSize);
    
    if (!m_buffer->isInUse())
      m_offset = 0;
    
    m_offset = dxvk::align(m_offset, align);

    if (m_offset + size > MaxBufferSize) {
      m_offset = 0;

      if (m_buffers.size() < MaxBufferCount)
        m_buffers.push(std::move(m_buffer));

      if (!m_buffers.front()->isInUse()) {
        m_buffer = std::move(m_buffers.front());
        m_buffers.pop();
      } else {
        m_buffer = createBuffer(MaxBufferSize);
      }
    }

    DxvkBufferSlice slice(m_buffer, m_offset, size);
    m_offset = dxvk::align(m_offset + size, align);
    return slice;
  }


  void DxvkStagingDataAlloc::trim() {
    m_buffer = nullptr;
    m_offset = 0;

    while (!m_buffers.empty())
      m_buffers.pop();
  }


  Rc<DxvkBuffer> DxvkStagingDataAlloc::createBuffer(VkDeviceSize size) {
    DxvkBufferCreateInfo info;
    info.size   = size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT;

    return m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
}
