#pragma once

#include "dxvk_format.h"
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
   * \brief Buffer view create info
   * 
   * The properties of a buffer view that
   * are to \ref DxvkDevice::createBufferView
   */
  struct DxvkBufferViewCreateInfo {
    /// Buffer data format, like image data
    VkFormat format;
    
    /// Offset of the buffer region to include in the view
    VkDeviceSize rangeOffset;
    
    /// Size of the buffer region to include in the view
    VkDeviceSize rangeLength;
  };
  
  
  /**
   * \brief Physical buffer resource
   * 
   * 
   */
  class DxvkBufferResource : public DxvkResource {
    
  public:
    
    DxvkBufferResource(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkBufferCreateInfo& createInfo,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    
    ~DxvkBufferResource();
    
    VkBuffer handle() const {
      return m_buffer;
    }
    
    void* mapPtr(VkDeviceSize offset) const {
      return m_memory.mapPtr(offset);
    }
    
  private:
    
    Rc<vk::DeviceFn>        m_vkd;
    DxvkMemory              m_memory;
    VkBuffer                m_buffer;
    
  };
  
  
  /**
   * \brief Virtual buffer resource
   * 
   * A simple buffer resource that stores linear,
   * unformatted data. Can be accessed by the host
   * if allocated on an appropriate memory type.
   */
  class DxvkBuffer : public RcObject {
    
  public:
    
    DxvkBuffer(
            DxvkDevice*           device,
      const DxvkBufferCreateInfo& createInfo,
            VkMemoryPropertyFlags memoryType);
    
    /**
     * \brief Buffer properties
     * \returns Buffer properties
     */
    const DxvkBufferCreateInfo& info() const {
      return m_info;
    }
    
    /**
     * \brief Buffer handle
     * \returns Buffer handle
     */
    VkBuffer handle() const {
      return m_resource->handle();;
    }
    
    /**
     * \brief Map pointer
     * 
     * If the buffer has been created on a host-visible
     * memory type, the buffer memory is mapped and can
     * be accessed by the host.
     * \param [in] offset Byte offset into mapped region
     * \returns Pointer to mapped memory region
     */
    void* mapPtr(VkDeviceSize offset) const {
      return m_resource->mapPtr(offset);
    }
    
    /**
     * \brief Checks whether the buffer is in use
     * 
     * Returns \c true if the underlying buffer resource
     * is in use. If it is, it should not be accessed by
     * the host for reading or writing, but reallocating
     * the buffer is a valid strategy to overcome this.
     * \returns \c true if the buffer is in use
     */
    bool isInUse() const {
      return m_resource->isInUse();
    }
    
    /**
     * \brief Underlying buffer resource
     * 
     * Use this for lifetime tracking.
     * \returns The resource object
     */
    Rc<DxvkResource> resource() const {
      return m_resource;
    }
    
    /**
     * \brief Replaces backing resource
     * 
     * Replaces the underlying buffer and implicitly marks
     * any buffer views using this resource as dirty. Do
     * not call this directly as this is called implicitly
     * by the context's \c invalidateBuffer method.
     * \param [in] resource The new backing resource
     */
    void renameResource(
      const Rc<DxvkBufferResource>& resource);
    
    /**
     * \brief Allocates new backing resource
     * \returns The new buffer
     */
    Rc<DxvkBufferResource> allocateResource();
    
  private:
    
    DxvkDevice*             m_device;
    DxvkBufferCreateInfo    m_info;
    VkMemoryPropertyFlags   m_memFlags;
    
    Rc<DxvkBufferResource>  m_resource;
    
  };
  
  
  /**
   * \brief Buffer view
   * 
   * Allows the application to interpret buffer
   * contents like formatted pixel data. These
   * buffer views are used as texel buffers.
   */
  class DxvkBufferView : public DxvkResource {
    
  public:
    
    DxvkBufferView(
      const Rc<vk::DeviceFn>&         vkd,
      const Rc<DxvkBuffer>&           buffer,
      const DxvkBufferViewCreateInfo& info);
    
    ~DxvkBufferView();
    
    /**
     * \brief Buffer view handle
     * \returns Buffer view handle
     */
    VkBufferView handle() const {
      return m_view;
    }
    
    /**
     * \brief Buffer view properties
     * \returns Buffer view properties
     */
    const DxvkBufferViewCreateInfo& info() const {
      return m_info;
    }
    
    /**
     * \brief Underlying buffer object
     * \returns Underlying buffer object
     */
    Rc<DxvkBuffer> buffer() const {
      return m_buffer;
    }
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    Rc<DxvkBuffer>    m_buffer;
    
    DxvkBufferViewCreateInfo m_info;
    VkBufferView             m_view;
    
  };
  
  
  /**
   * \brief Buffer slice
   * 
   * Stores the buffer and a sub-range of the buffer.
   * Slices are considered equal if the buffer and
   * the buffer range are the same.
   */
  class DxvkBufferSlice {
    
  public:
    
    DxvkBufferSlice() { }
    DxvkBufferSlice(
      const Rc<DxvkBuffer>& buffer,
            VkDeviceSize    rangeOffset,
            VkDeviceSize    rangeLength)
    : m_buffer(buffer),
      m_offset(rangeOffset),
      m_length(rangeLength) { }
    
    Rc<DxvkBuffer> buffer() const {
      return m_buffer;
    }
    
    Rc<DxvkResource> resource() const {
      return m_buffer->resource();
    }
    
    VkBuffer handle() const {
      return m_buffer != nullptr
        ? m_buffer->handle()
        : VK_NULL_HANDLE;
    }
    
    size_t offset() const {
      return m_offset;
    }
    
    size_t length() const {
      return m_length;
    }
    
    void* mapPtr(VkDeviceSize offset) const  {
      return m_buffer->mapPtr(m_offset + offset);
    }
    
    VkDescriptorBufferInfo descriptorInfo() const {
      VkDescriptorBufferInfo info;
      info.buffer = m_buffer->handle();
      info.offset = m_offset;
      info.range  = m_length;
      return info;
    }
    
    bool operator == (const DxvkBufferSlice& other) const {
      return this->m_buffer == other.m_buffer
          && this->m_offset == other.m_offset
          && this->m_length == other.m_length;
    }
    
    bool operator != (const DxvkBufferSlice& other) const {
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