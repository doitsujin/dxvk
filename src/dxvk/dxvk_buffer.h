#pragma once

#include <unordered_map>
#include <vector>

#include "dxvk_descriptor.h"
#include "dxvk_format.h"
#include "dxvk_hash.h"
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
   * \brief Buffer info
   * 
   * Stores a Vulkan buffer handle and the
   * memory object that is bound to the buffer.
   */
  struct DxvkBufferHandle {
    VkBuffer      buffer = VK_NULL_HANDLE;
    DxvkMemory    memory;
  };
  

  /**
   * \brief Buffer slice info
   * 
   * Stores the Vulkan buffer handle, offset
   * and length of the slice, and a pointer
   * to the mapped region..
   */
  struct DxvkBufferSliceHandle {
    VkBuffer      handle;
    VkDeviceSize  offset;
    VkDeviceSize  length;
    void*         mapPtr;

    bool eq(const DxvkBufferSliceHandle& other) const {
      return handle == other.handle
          && offset == other.offset
          && length == other.length;
    }

    size_t hash() const {
      DxvkHashState result;
      result.add(std::hash<VkBuffer>()(handle));
      result.add(std::hash<VkDeviceSize>()(offset));
      result.add(std::hash<VkDeviceSize>()(length));
      return result;
    }
  };

  
  /**
   * \brief Virtual buffer resource
   * 
   * A simple buffer resource that stores linear,
   * unformatted data. Can be accessed by the host
   * if allocated on an appropriate memory type.
   */
  class DxvkBuffer : public DxvkResource {
    friend class DxvkBufferView;
  public:
    
    DxvkBuffer(
            DxvkDevice*           device,
      const DxvkBufferCreateInfo& createInfo,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    
    ~DxvkBuffer();
    
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
      return reinterpret_cast<char*>(m_physSlice.mapPtr) + offset;
    }
    
    /**
     * \brief Retrieves slice handle
     * \returns Buffer slice handle
     */
    DxvkBufferSliceHandle getSliceHandle() const {
      return m_physSlice;
    }

    /**
     * \brief Retrieves sub slice handle
     * 
     * \param [in] offset Offset into buffer
     * \param [in] length Sub slice length
     * \returns Buffer slice handle
     */
    DxvkBufferSliceHandle getSliceHandle(VkDeviceSize offset, VkDeviceSize length) const {
      DxvkBufferSliceHandle result;
      result.handle = m_physSlice.handle;
      result.offset = m_physSlice.offset + offset;
      result.length = length;
      result.mapPtr = mapPtr(offset);
      return result;
    }

    /**
     * \brief Retrieves descriptor info
     * 
     * \param [in] offset Buffer slice offset
     * \param [in] length Buffer slice length
     * \returns Buffer slice descriptor
     */
    DxvkDescriptorInfo getDescriptor(VkDeviceSize offset, VkDeviceSize length) const {
      DxvkDescriptorInfo result;
      result.buffer.buffer = m_physSlice.handle;
      result.buffer.offset = m_physSlice.offset + offset;
      result.buffer.range  = length;
      return result;
    }

    /**
     * \brief Retrieves dynamic offset
     * 
     * \param [in] offset Offset into the buffer
     * \returns Offset for dynamic descriptors
     */
    VkDeviceSize getDynamicOffset(VkDeviceSize offset) const {
      return m_physSlice.offset + offset;
    }
    
    /**
     * \brief Replaces backing resource
     * 
     * Replaces the underlying buffer and implicitly marks
     * any buffer views using this resource as dirty. Do
     * not call this directly as this is called implicitly
     * by the context's \c invalidateBuffer method.
     * \param [in] slice The new backing resource
     * \returns Previous buffer slice
     */
    DxvkBufferSliceHandle rename(const DxvkBufferSliceHandle& slice) {
      return std::exchange(m_physSlice, slice);
    }
    
    /**
     * \brief Transform feedback vertex stride
     * 
     * Used when drawing after transform feedback,
     * \returns The current xfb vertex stride
     */
    uint32_t getXfbVertexStride() const {
      return m_vertexStride;
    }
    
    /**
     * \brief Set transform feedback vertex stride
     * 
     * When the buffer is used as a transform feedback
     * buffer, this will be set to the vertex stride
     * defined by the geometry shader.
     * \param [in] stride Vertex stride
     */
    void setXfbVertexStride(uint32_t stride) {
      m_vertexStride = stride;
    }
    
    /**
     * \brief Allocates new buffer slice
     * \returns The new buffer slice
     */
    DxvkBufferSliceHandle allocSlice();
    
    /**
     * \brief Frees a buffer slice
     * 
     * Marks the slice as free so that it can be used for
     * subsequent allocations. Called automatically when
     * the slice is no longer needed by the GPU.
     * \param [in] slice The buffer slice to free
     */
    void freeSlice(
      const DxvkBufferSliceHandle& slice);
    
  private:

    DxvkDevice*             m_device;
    DxvkBufferCreateInfo    m_info;
    DxvkMemoryAllocator*    m_memAlloc;
    VkMemoryPropertyFlags   m_memFlags;
    
    DxvkBufferHandle        m_buffer;
    DxvkBufferSliceHandle   m_physSlice;

    uint32_t                m_vertexStride = 0;
    
    sync::Spinlock m_freeMutex;
    sync::Spinlock m_swapMutex;
    
    std::vector<DxvkBufferHandle>        m_buffers;
    std::vector<DxvkBufferSliceHandle>   m_freeSlices;
    std::vector<DxvkBufferSliceHandle>   m_nextSlices;
    
    VkDeviceSize m_physSliceLength  = 0;
    VkDeviceSize m_physSliceStride  = 0;
    VkDeviceSize m_physSliceCount   = 2;

    DxvkBufferHandle allocBuffer(
            VkDeviceSize          sliceCount) const;
    
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
    const Rc<DxvkBuffer>& buffer() const {
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
     * \brief Retrieves buffer slice handle
     * 
     * Returns the buffer handle and offset
     * \returns Buffer slice handle
     */
    DxvkBufferSliceHandle getSliceHandle() const {
      return m_buffer != nullptr
        ? m_buffer->getSliceHandle(m_offset, m_length)
        : DxvkBufferSliceHandle();
    }

    /**
     * \brief Retrieves sub slice handle
     * 
     * \param [in] offset Offset into buffer
     * \param [in] length Sub slice length
     * \returns Buffer slice handle
     */
    DxvkBufferSliceHandle getSliceHandle(VkDeviceSize offset, VkDeviceSize length) const {
      return m_buffer != nullptr
        ? m_buffer->getSliceHandle(m_offset + offset, length)
        : DxvkBufferSliceHandle();
    }

    /**
     * \brief Retrieves descriptor info
     * \returns Buffer slice descriptor
     */
    DxvkDescriptorInfo getDescriptor() const {
      return m_buffer->getDescriptor(m_offset, m_length);
    }

    /**
     * \brief Retrieves dynamic offset
     * 
     * Used for descriptor set binding.
     * \returns Buffer slice offset
     */
    VkDeviceSize getDynamicOffset() const {
      return m_buffer->getDynamicOffset(m_offset);
    }
    
    /**
     * \brief Pointer to mapped memory region
     * 
     * \param [in] offset Offset into the slice
     * \returns Pointer into mapped buffer memory
     */
    void* mapPtr(VkDeviceSize offset) const  {
      return m_buffer != nullptr
        ? m_buffer->mapPtr(m_offset + offset)
        : nullptr;
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
      return m_bufferView;
    }
    
    /**
     * \brief Element cound
     * 
     * Number of typed elements contained
     * in the buffer view. Depends on the
     * buffer view format.
     * \returns Element count
     */
    VkDeviceSize elementCount() const {
      auto format = imageFormatInfo(m_info.format);
      return m_info.rangeLength / format->elementSize;
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
    const Rc<DxvkBuffer>& buffer() const {
      return m_buffer;
    }
    
    /**
     * \brief Underlying buffer info
     * \returns Underlying buffer info
     */
    const DxvkBufferCreateInfo& bufferInfo() const {
      return m_buffer->info();
    }

    /**
     * \brief Retrieves buffer slice handle
     * \returns Buffer slice handle
     */
    DxvkBufferSliceHandle getSliceHandle() const {
      return m_buffer->getSliceHandle(
        m_info.rangeOffset,
        m_info.rangeLength);
    }
    
    /**
     * \brief Underlying buffer slice
     * \returns Slice backing the view
     */
    DxvkBufferSlice slice() const {
      return DxvkBufferSlice(m_buffer,
        m_info.rangeOffset,
        m_info.rangeLength);
    }
    
    /**
     * \brief Updates the buffer view
     * 
     * If the buffer has been invalidated ever since
     * the view was created, the view is invalid as
     * well and needs to be re-created. Call this
     * prior to using the buffer view handle.
     */
    void updateView() {
      DxvkBufferSliceHandle slice = getSliceHandle();

      if (!m_bufferSlice.eq(slice))
        this->updateBufferView(slice);
    }
    
  private:
    
    Rc<vk::DeviceFn>          m_vkd;
    DxvkBufferViewCreateInfo  m_info;
    Rc<DxvkBuffer>            m_buffer;

    DxvkBufferSliceHandle     m_bufferSlice;
    VkBufferView              m_bufferView;

    std::unordered_map<
      DxvkBufferSliceHandle,
      VkBufferView,
      DxvkHash, DxvkEq> m_views;
    
    VkBufferView createBufferView(
      const DxvkBufferSliceHandle& slice);
    
    void updateBufferView(
      const DxvkBufferSliceHandle& slice);
    
  };
  
  
  /**
   * \brief Buffer slice tracker
   * 
   * Stores a list of buffer slices that can be
   * freed. Useful when buffers have been renamed
   * and the original slice is no longer needed.
   */
  class DxvkBufferTracker {
    
  public:
    
    DxvkBufferTracker();
    ~DxvkBufferTracker();
    
    void freeBufferSlice(
      const Rc<DxvkBuffer>&         buffer,
      const DxvkBufferSliceHandle&  slice);
    
    void reset();
    
  private:
    
    struct Entry {
      Rc<DxvkBuffer>        buffer;
      DxvkBufferSliceHandle slice;
    };
    
    std::vector<Entry> m_entries;
    
  };
  
}