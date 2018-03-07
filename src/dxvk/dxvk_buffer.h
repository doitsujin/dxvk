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
    friend class DxvkBufferView;
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
     * \brief Map pointer
     * 
     * If the buffer has been created on a host-visible
     * memory type, the buffer memory is mapped and can
     * be accessed by the host.
     * \param [in] offset Byte offset into mapped region
     * \returns Pointer to mapped memory region
     */
    void* mapPtr(VkDeviceSize offset) const {
      return m_physSlice.mapPtr(offset);
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
      return m_physSlice.resource()->isInUse();
    }
    
    /**
     * \brief Underlying buffer resource
     * 
     * Use this for lifetime tracking.
     * \returns The resource object
     */
    Rc<DxvkResource> resource() const {
      return m_physSlice.resource();
    }
    
    /**
     * \brief Physical buffer slice
     * 
     * Retrieves a slice into the physical
     * buffer which backs this buffer.
     * \returns The backing slice
     */
    DxvkPhysicalBufferSlice slice() const {
      return m_physSlice;
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
      return m_physSlice.subSlice(offset, length);
    }
    
    /**
     * \brief Replaces backing resource
     * 
     * Replaces the underlying buffer and implicitly marks
     * any buffer views using this resource as dirty. Do
     * not call this directly as this is called implicitly
     * by the context's \c invalidateBuffer method.
     * \param [in] slice The new backing resource
     */
    void rename(
      const DxvkPhysicalBufferSlice& slice);
    
    /**
     * \brief Allocates new physical resource
     * 
     * This method must not be called from multiple threads
     * simultaneously, but it can be called in parallel with
     * \ref rename and other methods of this class.
     * \returns The new backing buffer slice
     */
    DxvkPhysicalBufferSlice allocPhysicalSlice();
    
  private:
    
    DxvkDevice*             m_device;
    DxvkBufferCreateInfo    m_info;
    VkMemoryPropertyFlags   m_memFlags;
    DxvkPhysicalBufferSlice m_physSlice;
    uint32_t                m_revision = 0;
    
    // TODO maybe align this to a cache line in order
    // to avoid false sharing once CSMT is implemented
    VkDeviceSize m_physBufferId     = 0;
    VkDeviceSize m_physSliceId      = 0;
    VkDeviceSize m_physSliceCount   = 1;
    VkDeviceSize m_physSliceLength  = 0;
    VkDeviceSize m_physSliceStride  = 0;
    
    std::array<Rc<DxvkPhysicalBuffer>, 2> m_physBuffers;
    
    Rc<DxvkPhysicalBuffer> allocPhysicalBuffer(
            VkDeviceSize    sliceCount) const;
    
  };
  
  
  /**
   * \brief Buffer view
   * 
   * Allows the application to interpret buffer
   * contents like formatted pixel data. These
   * buffer views are used as texel buffers.
   */
  class DxvkBufferView : public RcObject {
    
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
      return m_physView->handle();
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
    const DxvkBufferCreateInfo& bufferInfo() const {
      return m_buffer->info();
    }
    
    /**
     * \brief Backing resource
     * \returns Backing resource
     */
    Rc<DxvkResource> viewResource() const {
      return m_physView;
    }
    
    /**
     * \brief Backing buffer resource
     * \returns Backing buffer resource
     */
    Rc<DxvkResource> bufferResource() const {
      return m_physView->slice().resource();
    }
    
    /**
     * \brief Underlying buffer slice
     * \returns Slice backing the view
     */
    DxvkPhysicalBufferSlice slice() const {
      return m_physView->slice();
    }
    
    /**
     * \brief Updates the buffer view
     * 
     * If the buffer has been invalidated ever since
     * the view was created, the view is invalid as
     * well and needs to be re-created. Call this
     * prior to using the buffer view handle.
     */
    void updateView();
    
  private:
    
    Rc<vk::DeviceFn>           m_vkd;
    DxvkBufferViewCreateInfo   m_info;
    
    Rc<DxvkBuffer>             m_buffer;
    Rc<DxvkPhysicalBufferView> m_physView;
    
    uint32_t                   m_revision = 0;
    
    Rc<DxvkPhysicalBufferView> createView();
    
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
    
    explicit DxvkBufferSlice(const Rc<DxvkBuffer>& buffer)
    : DxvkBufferSlice(buffer, 0, buffer->info().size) { }
    
    size_t offset() const { return m_offset; }
    size_t length() const { return m_length; }
    
    /**
     * \brief Underlying buffer
     * \returns The virtual buffer
     */
    Rc<DxvkBuffer> buffer() const {
      return m_buffer;
    }
    
    /**
     * \brief Buffer info
     * 
     * Retrieves the properties of the underlying
     * virtual buffer. Should not be used directly
     * by client APIs.
     * \returns Buffer properties
     */
    const DxvkBufferCreateInfo& bufferInfo() const {
      return m_buffer->info();
    }
    
    /**
     * \brief Buffer sub slice
     * 
     * Takes a sub slice from this slice.
     * \param [in] offset Sub slice offset
     * \param [in] length Sub slice length
     * \returns The sub slice object
     */
    DxvkBufferSlice subSlice(VkDeviceSize offset, VkDeviceSize length) const {
      return DxvkBufferSlice(m_buffer, offset, length);
    }
    
    /**
     * \brief Checks whether the slice is valid
     * 
     * A buffer slice that does not point to any virtual
     * buffer object is considered undefined and cannot
     * be used for any operations.
     * \returns \c true if the slice is defined
     */
    bool defined() const {
      return m_buffer != nullptr;
    }
    
    /**
     * \brief Physical slice
     * 
     * Retrieves the physical slice that currently
     * backs the virtual slice. This may change
     * when the virtual buffer gets invalidated.
     * \returns The physical buffer slice
     */
    DxvkPhysicalBufferSlice physicalSlice() const {
      return m_buffer->subSlice(m_offset, m_length);
    }
    
    /**
     * \brief Pointer to mapped memory region
     * 
     * \param [in] offset Offset into the slice
     * \returns Pointer into mapped buffer memory
     */
    void* mapPtr(VkDeviceSize offset) const  {
      return m_buffer->mapPtr(m_offset + offset);
    }
    
    /**
     * \brief Checks whether two slices are equal
     * 
     * Two slices are considered equal if they point to
     * the same memory region within the same buffer.
     * \param [in] other The slice to compare to
     * \returns \c true if the two slices are the same
     */
    bool matches(const DxvkBufferSlice& other) const {
      return this->m_buffer == other.m_buffer
          && this->m_offset == other.m_offset
          && this->m_length == other.m_length;
    }
    
  private:
    
    Rc<DxvkBuffer> m_buffer = nullptr;
    VkDeviceSize   m_offset = 0;
    VkDeviceSize   m_length = 0;
    
  };
  
}