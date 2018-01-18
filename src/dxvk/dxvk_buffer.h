#pragma once

#include "dxvk_buffer_res.h"

namespace dxvk {
  
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
     * \brief Memory type flags
     * 
     * Use this to determine whether a
     * buffer is mapped to host memory.
     * \returns Vulkan memory flags
     */
    VkMemoryPropertyFlags memFlags() const {
      return m_memFlags;
    }
    
    /**
     * \brief Buffer handle
     * \returns Buffer handle
     */
    VkBuffer handle() const {
      return m_resource.handle();
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
      return m_resource.mapPtr(offset);
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
      return m_resource.resource()->isInUse();
    }
    
    /**
     * \brief Underlying buffer resource
     * 
     * Use this for lifetime tracking.
     * \returns The resource object
     */
    Rc<DxvkResource> resource() const {
      return m_resource.resource();
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
      const DxvkPhysicalBufferSlice& resource);
    
    /**
     * \brief Allocates new backing resource
     * \returns The new buffer
     */
    DxvkPhysicalBufferSlice allocateResource();
    
    /**
     * \brief Physical buffer slice
     * 
     * Retrieves a slice into the physical
     * buffer which backs this buffer.
     * \returns The backing slice
     */
    DxvkPhysicalBufferSlice slice() const {
      return m_resource;
    }
    
    /**
     * \brief Physical buffer sub slice
     * 
     * Retrieves a sub slice into the backing buffer.
     * \param [in] offset Offset into the buffer
     * \param [in] length Length of the slice
     * \returns The sub slice
     */
    DxvkPhysicalBufferSlice subSlice(VkDeviceSize offset, VkDeviceSize length) const {
      return m_resource.subSlice(offset, length);
    }
    
  private:
    
    DxvkDevice*             m_device;
    DxvkBufferCreateInfo    m_info;
    VkMemoryPropertyFlags   m_memFlags;
    
    DxvkPhysicalBufferSlice m_resource;
    
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
    
    /**
     * \brief Underlying buffer slice
     * \returns Slice backing the view
     */
    DxvkPhysicalBufferSlice slice() const {
      return m_buffer->subSlice(
        m_info.rangeOffset,
        m_info.rangeLength);
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
    
    explicit DxvkBufferSlice(const Rc<DxvkBuffer>& buffer)
    : DxvkBufferSlice(buffer, 0, buffer->info().size) { }
    
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
    
    VkMemoryPropertyFlags memFlags() const {
      return m_buffer != nullptr
        ? m_buffer->memFlags()
        : 0;
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
    
    DxvkPhysicalBufferSlice physicalSlice() const {
      return m_buffer->subSlice(m_offset, m_length);
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