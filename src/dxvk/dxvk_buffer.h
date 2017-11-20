#pragma once

#include "dxvk_memory.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Buffer create info
   * 
   * The properties of a buffer that are
   * passed to \ref DxvkDevice::createBuffer
   */
  struct DxvkBufferCreateInfo {
    
    /// Size of the buffer, in bytes
    VkDeviceSize size;
    
    /// Buffer usage flags
    VkBufferUsageFlags usage;
    
    /// Pipeline stages that can access
    /// the contents of the buffer.
    VkPipelineStageFlags stages;
    
    /// Allowed access patterns
    VkAccessFlags access;
    
  };
  
  
  /**
   * \brief DXVK buffer
   * 
   * A simple buffer resource that stores
   * linear data. Can be mapped to host
   * memory.
   */
  class DxvkBuffer : public DxvkResource {
    
  public:
    
    DxvkBuffer(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkBufferCreateInfo& createInfo,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    ~DxvkBuffer();
    
    /**
     * \brief Buffer handle
     * \returns Buffer handle
     */
    VkBuffer handle() const {
      return m_buffer;
    }
    
    /**
     * \brief Buffer properties
     * \returns Buffer properties
     */
    const DxvkBufferCreateInfo& info() const {
      return m_info;
    }
    
    /**
     * \brief Map pointer
     * 
     * If the buffer has been created on a host-visible
     * memory type, the buffer memory is mapped and can
     * be accessed by the host.
     * \returns Pointer to mapped memory region
     */
    void* mapPtr() const {
      return m_memory.mapPtr();
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    DxvkBufferCreateInfo  m_info;
    DxvkMemory            m_memory;
    VkBuffer              m_buffer = VK_NULL_HANDLE;
    
  };
  
  
  /**
   * \brief Buffer binding
   * 
   * Stores the buffer and the sub-range of the buffer
   * to bind. Bindings are considered equal if all three
   * parameters are the same.
   */
  class DxvkBufferBinding {
    
  public:
    
    DxvkBufferBinding() { }
    DxvkBufferBinding(
      const Rc<DxvkBuffer>& buffer,
            VkDeviceSize    rangeOffset,
            VkDeviceSize    rangeLength)
    : m_buffer(buffer),
      m_offset(rangeOffset),
      m_length(rangeLength) { }
    
    Rc<DxvkResource> resource() const {
      return m_buffer;
    }
    
    VkBuffer bufferHandle() const {
      return m_buffer != nullptr
        ? m_buffer->handle()
        : VK_NULL_HANDLE;
    }
    
    size_t bufferOffset() const {
      return m_offset;
    }
    
    size_t bufferRange() const {
      return m_length;
    }
    
    VkDescriptorBufferInfo descriptorInfo() const {
      VkDescriptorBufferInfo info;
      info.buffer = m_buffer->handle();
      info.offset = m_offset;
      info.range  = m_length;
      return info;
    }
    
    bool operator == (const DxvkBufferBinding& other) const {
      return this->m_buffer == other.m_buffer
          && this->m_offset == other.m_offset
          && this->m_length == other.m_length;
    }
    
    bool operator != (const DxvkBufferBinding& other) const {
      return this->m_buffer != other.m_buffer
          || this->m_offset != other.m_offset
          || this->m_length != other.m_length;
    }
    
  private:
    
    Rc<DxvkBuffer> m_buffer = nullptr;
    VkDeviceSize   m_offset = 0;
    VkDeviceSize   m_length = 0;
    
  };
  
}