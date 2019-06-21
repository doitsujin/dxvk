#pragma once

#include <queue>

#include "dxvk_buffer.h"

namespace dxvk {
  
  class DxvkDevice;
  
  /**
   * \brief Staging buffer slice
   * 
   * Provides the application with a
   * pointer to the mapped buffer.
   */
  struct DxvkStagingBufferSlice {
    VkBuffer      buffer = VK_NULL_HANDLE;
    VkDeviceSize  offset = 0;
    void*         mapPtr = nullptr;
  };
  
  
  /**
   * \brief Staging uffer
   * 
   * A mapped buffer that can be used for fast data
   * transfer operations from the host to the GPU.
   * Implements a linear sub-allocator for staging
   * buffer slices.
   */
  class DxvkStagingBuffer : public RcObject {
    
  public:
    
    DxvkStagingBuffer(
      const Rc<DxvkBuffer>& buffer);
    ~DxvkStagingBuffer();
    
    /**
     * \brief Buffer size, in bytes
     * \returns Buffer size, in bytes
     */
    VkDeviceSize size() const;
    
    /**
     * \brief Number of bytes still available
     * \returns Number of bytes still available
     */
    VkDeviceSize freeBytes() const;
    
    /**
     * \brief Allocates a staging buffer slice
     * 
     * This may fail if the amount of data requested is
     * larger than the amount of data still available.
     * \param [in] size Requested allocation size
     * \param [out] slice Allocated staging buffer slice
     * \returns \c true on success, \c false on failure
     */
    bool alloc(
            VkDeviceSize            size,
            DxvkStagingBufferSlice& slice);
    
    /**
     * \brief Resets staging buffer
     * 
     * Resets the allocator and thus frees
     * all slices allocated from the buffer.
     */
    void reset();
    
  private:
    
    Rc<DxvkBuffer> m_buffer;
    
    VkDeviceSize m_bufferSize   = 0;
    VkDeviceSize m_bufferOffset = 0;
    
  };
  
  
  /**
   * \brief Staging buffer allocator
   * 
   * Convenient allocator for staging buffer slices
   * which creates new staging buffers on demand.
   */
  class DxvkStagingAlloc {
    
  public:
    
    DxvkStagingAlloc(DxvkDevice* device);
    ~DxvkStagingAlloc();
    
    /**
     * \brief Allocates a staging buffer slice
     * 
     * This \e may create a new staging buffer
     * if needed. This method should not fail.
     * \param [in] size Required amount of memory
     * \returns Allocated staging buffer slice
     */
    DxvkStagingBufferSlice alloc(
            VkDeviceSize      size);
    
    /**
     * \brief Resets staging buffer allocator
     * 
     * Returns all buffers to the device so that
     * they can be recycled. Buffers must not be
     * in use when this is called.
     */
    void reset();
    
  private:
    
    DxvkDevice* const m_device;
    
    std::vector<Rc<DxvkStagingBuffer>> m_stagingBuffers;
    
  };


  /**
   * \brief Staging data allocator
   *
   * Allocates buffer slices for resource uploads,
   * while trying to keep the number of allocations
   * but also the amount of allocated memory low.
   */
  class DxvkStagingDataAlloc {
    constexpr static VkDeviceSize MaxBufferSize  = 1 << 24; // 16 MiB
    constexpr static uint32_t     MaxBufferCount = 2;
  public:

    DxvkStagingDataAlloc(const Rc<DxvkDevice>& device);

    ~DxvkStagingDataAlloc();

    /**
     * \brief Alloctaes a staging buffer slice
     * 
     * \param [in] align Alignment of the allocation
     * \param [in] size Size of the allocation
     * \returns Staging buffer slice
     */
    DxvkBufferSlice alloc(VkDeviceSize align, VkDeviceSize size);

  private:

    Rc<DxvkDevice>  m_device;
    Rc<DxvkBuffer>  m_buffer;
    VkDeviceSize    m_offset = 0;

    std::queue<Rc<DxvkBuffer>> m_buffers;

    Rc<DxvkBuffer> createBuffer(VkDeviceSize size);

  };
  
}
