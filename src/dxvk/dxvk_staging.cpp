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
  
}
