#pragma once

#include "dxvk_buffer.h"

namespace dxvk {
  
  class DxvkDevice;
  
  struct DxvkStagingBufferSlice {
    VkBuffer      buffer = VK_NULL_HANDLE;
    VkDeviceSize  offset = 0;
    void*         mapPtr = nullptr;
  };
  
  
  class DxvkStagingBuffer : public RcObject {
    
  public:
    
    DxvkStagingBuffer(
      const Rc<DxvkBuffer>& buffer);
    
    ~DxvkStagingBuffer();
    
    VkDeviceSize freeBytes() const;
    
    bool alloc(
            VkDeviceSize            size,
            DxvkStagingBufferSlice& slice);
    
    void reset();
    
  private:
    
    Rc<DxvkBuffer> m_buffer;
    
    VkDeviceSize m_bufferSize   = 0;
    VkDeviceSize m_bufferOffset = 0;
    
  };
  
  
  class DxvkStagingAlloc {
    
  public:
    
    DxvkStagingAlloc(DxvkDevice* device);
    ~DxvkStagingAlloc();
    
    DxvkStagingBufferSlice alloc(
            VkDeviceSize      size);
    
    void reset();
    
  private:
    
    DxvkDevice* const m_device;
    
    std::vector<Rc<DxvkStagingBuffer>> m_stagingBuffers;
    
  };
  
}
