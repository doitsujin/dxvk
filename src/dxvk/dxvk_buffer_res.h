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
  
  
  class DxvkPhysicalBuffer;
  class DxvkPhysicalBufferSlice;
  
  
  /**
   * \brief Physical buffer
   * 
   * A physical buffer is used as a backing resource for
   * a virtual buffer. See \ref DxvkBuffer as for why
   * this separation is necessary.
   */
  class DxvkPhysicalBuffer : public DxvkResource {
    
  public:
    
    DxvkPhysicalBuffer(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkBufferCreateInfo& createInfo,
            VkDeviceSize          sliceCount,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    
    ~DxvkPhysicalBuffer();
    
    /**
     * \brief Vulkan buffer handle
     * \returns Vulkan buffer handle
     */
    VkBuffer handle() const {
      return m_handle;
    }
    
    /**
     * \brief Map pointer
     * 
     * Retrieves a pointer into the mapped memory region
     * of the buffer, relative to the start of the buffer.
     * \param [in] offset Offset into the buffer
     * \returns Pointer into the mapped memory region
     */
    void* mapPtr(VkDeviceSize offset) const {
      return m_memory.mapPtr(offset);
    }
    
    /**
     * \brief Retrieves a physical buffer slice
     * 
     * Returns the buffer object and the offset of the
     * given slice. Slices are always aligned to the
     * highest required alignment of the device, so
     * that they can be used for any purpose.
     * \param [in] id Slice index
     * \returns The physical slice
     */
    DxvkPhysicalBufferSlice slice(uint32_t id);
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    DxvkMemory        m_memory;
    VkBuffer          m_handle;
    
    VkDeviceSize      m_sliceLength;
    VkDeviceSize      m_sliceStride;
    
  };
  
  
  /**
   * \brief Physical buffer slice
   * 
   * A slice into a physical buffer, which stores
   * the buffer and the offset into the buffer.
   */
  class DxvkPhysicalBufferSlice {
    
  public:
    
    DxvkPhysicalBufferSlice() { }
    DxvkPhysicalBufferSlice(
      const Rc<DxvkPhysicalBuffer>& buffer,
            VkDeviceSize            offset,
            VkDeviceSize            length)
    : m_buffer(buffer),
      m_offset(offset),
      m_length(length) { }
    
    /**
     * \brief Buffer handle
     * \returns Buffer handle
     */
    VkBuffer handle() const {
      return m_buffer->handle();
    }
    
    /**
     * \brief Slice offset
     * 
     * Offset of the slice into
     * the underlying buffer.
     * \returns Slice offset
     */
    VkDeviceSize offset() const {
      return m_offset;
    }
    
    /**
     * \brief Slice length
     * 
     * Number of bytes in the slice.
     * \returns Slice length, in bytes
     */
    VkDeviceSize length() const {
      return m_length;
    }
    
    /**
     * \brief Sub slice into the physical buffer
     * 
     * \param [in] offset Offset, relative to this slice
     * \param [in] length Number of bytes of the sub slice
     * \returns The sub slice
     */
    DxvkPhysicalBufferSlice subSlice(VkDeviceSize offset, VkDeviceSize length) const {
      return DxvkPhysicalBufferSlice(m_buffer, m_offset + offset, length);
    }
    
    /**
     * \brief Map pointer
     * 
     * Retrieves a pointer into the mapped memory
     * region of the underlying buffer, relative
     * to the slice's offset.
     * \param [in] offset Offset into the slice
     * \returns Pointer to the mapped memory region
     */
    void* mapPtr(VkDeviceSize offset) const {
      return m_buffer->mapPtr(m_offset + offset);
    }
    
    /**
     * \brief The buffer resource
     * \returns Buffer resource
     */
    Rc<DxvkResource> resource() const {
      return m_buffer;
    }
    
  private:
    
    Rc<DxvkPhysicalBuffer> m_buffer;
    VkDeviceSize           m_offset;
    VkDeviceSize           m_length;
    
  };
  
}