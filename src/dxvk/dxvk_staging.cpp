#include "dxvk_device.h"
#include "dxvk_staging.h"

namespace dxvk {
  
  DxvkStagingBuffer::DxvkStagingBuffer(
    const Rc<DxvkBuffer>& buffer)
  : m_buffer(buffer), m_bufferSize(buffer->info().size){
    
  }
  
  
  DxvkStagingBuffer::~DxvkStagingBuffer() {
    
  }
  
  
  VkDeviceSize DxvkStagingBuffer::size() const {
    return m_bufferSize;
  }
  
  
  VkDeviceSize DxvkStagingBuffer::freeBytes() const {
    return m_bufferSize >= m_bufferOffset
      ? m_bufferSize - m_bufferOffset
      : VkDeviceSize(0);
  }
  
  
  bool DxvkStagingBuffer::alloc(
          VkDeviceSize            size,
          DxvkStagingBufferSlice& slice) {
    if (m_bufferOffset + size > m_bufferSize)
      return false;
    
    auto physSlice = m_buffer->getSliceHandle(m_bufferOffset, size);
    slice.buffer = physSlice.handle;
    slice.offset = physSlice.offset;
    slice.mapPtr = physSlice.mapPtr;
    
    m_bufferOffset = align(m_bufferOffset + size, 64);
    return true;
  }
  
  
  void DxvkStagingBuffer::reset() {
    m_bufferOffset = 0;
  }
  
  
  DxvkStagingAlloc::DxvkStagingAlloc(DxvkDevice* device)
  : m_device(device) { }
  
  
  DxvkStagingAlloc::~DxvkStagingAlloc() {
    this->reset();
  }
  
  
  DxvkStagingBufferSlice DxvkStagingAlloc::alloc(VkDeviceSize size) {
    Rc<DxvkStagingBuffer> selectedBuffer;
    
    // Try a worst-fit allocation strategy on the existing staging
    // buffers first. This should keep the amount of wasted space
    // small, especially if there are large allocations.
    for (const auto& buf : m_stagingBuffers) {
      if (selectedBuffer == nullptr || (buf->freeBytes() > selectedBuffer->freeBytes()))
        selectedBuffer = buf;
    }
    
    // If we have no suitable buffer, allocate one from the device
    // that is *at least* as large as the amount of data we need
    // to upload. Usually it will be bigger.
    DxvkStagingBufferSlice slice;
    
    if ((selectedBuffer == nullptr) || (!selectedBuffer->alloc(size, slice))) {
      selectedBuffer = m_device->allocStagingBuffer(size);
      selectedBuffer->alloc(size, slice);
      m_stagingBuffers.push_back(selectedBuffer);
    }
    
    return slice;
  }
  
  
  void DxvkStagingAlloc::reset() {
    for (const auto& buf : m_stagingBuffers)
      m_device->recycleStagingBuffer(buf);
    
    m_stagingBuffers.resize(0);
  }


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
