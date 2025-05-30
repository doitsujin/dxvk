#pragma once

#include <list>

#include "dxvk_buffer.h"
#include "dxvk_descriptor_info.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkResourceDescriptorHeap;

  /**
   * \brief Descriptor heap binding info
   *
   * Stores buffer properties for the purpose of
   * binding the descriptor heap.
   */
  struct DxvkDescriptorHeapBindingInfo {
    VkBuffer        buffer        = VK_NULL_HANDLE;
    VkDeviceAddress gpuAddress    = 0u;
    VkDeviceSize    reservedSize  = 0u;
    VkDeviceSize    bufferSize    = 0u;
  };


  /**
   * \brief Resource descriptor range
   *
   * Provides a reference-counted descriptor range that is suballocated from
   * a larger buffer. The intention is that each range provides a linear
   * allocator to allocate descriptors from, and each buffer serves as a ring
   * buffer that can be bound with the same base address.
   */
  class DxvkResourceDescriptorRange {
    friend class DxvkResourceDescriptorHeap;
  public:

    DxvkResourceDescriptorRange(
            DxvkResourceDescriptorHeap*         heap,
            Rc<DxvkBuffer>                      gpuBuffer,
            VkDeviceSize                        rangeSize,
            VkDeviceSize                        rangeIndex,
            VkDeviceSize                        reservedSize);

    ~DxvkResourceDescriptorRange();

    void incRef();
    void decRef();

    /**
     * \brief Checks whether any live references to this range exist
     *
     * Live references consider both CPU-side usage as well as GPU
     * usage tracking. If this returns \c false for any given range,
     * that range is guaranteed to be safe to use for allocations.
     * \returns \c true if the descriptor range is in use
     */
    bool isInUse() const {
      return m_useCount.load(std::memory_order_relaxed) != 0u;
    }

    /**
     * \brief Queries current allocation offset
     *
     * Primarily useful for statistics.
     * \returns Current allocation offset
     */
    VkDeviceSize getAllocationOffset() const {
      return m_allocationOffset;
    }

    /**
     * \brief Queries descriptor heap info
     *
     * Returns the base address of the descriptor heap rather than the
     * address of the specific slice. This is done to only bind each
     * buffer once.
     * \returns Buffer slice for descriptor heap binding
     */
    DxvkDescriptorHeapBindingInfo getHeapInfo() const {
      DxvkDescriptorHeapBindingInfo result = { };
      result.buffer = m_rangeInfo.buffer;
      result.gpuAddress = m_rangeInfo.gpuAddress - m_rangeInfo.offset;
      result.reservedSize = m_reservedSize;
      result.bufferSize = m_bufferSize;
      return result;
    }

    /**
     * \brief Queries underlying buffer ranges
     * \returns Buffer slice covered by the range
     */
    DxvkResourceBufferInfo getRangeInfo() const {
      return m_gpuBuffer->getSliceInfo(m_rangeOffset, m_rangeSize);
    }

    /**
     * \brief Checks whether the range can service an allocation
     *
     * Use this to test in advance whether this range has enough space for
     * descriptor sets with a combined size less than or equal to \c size.
     * If this returns \c true, such allocations are guaranteed to succeed,
     * otherwise a new range must be allocated from the heap.
     * \param [in] size Maximum size of sets to allocate
     * \returns \c true if there is enough space, \c false otherwise.
     */
    bool testAllocation(VkDeviceSize size) const {
      return m_allocationOffset + size <= m_rangeSize;
    }

    /**
     * \brief Allocates descriptor memory from the range
     *
     * Must only be used after verifying that the range has enough memory
     * left to service the allocation. \c size must be a multiple of the
     * maximum required descriptor set alignment.
     * \param [in] size Size of the set to allocate
     * \returns Properties of the allocated GPU buffer slice, but
     *    with the map pointer from the host buffer slice.
     */
    DxvkResourceBufferInfo alloc(VkDeviceSize size) {
      DxvkResourceBufferInfo result = { };
      result.buffer = m_rangeInfo.buffer;
      result.offset = m_rangeInfo.offset + m_allocationOffset;
      result.size = size;
      result.mapPtr = reinterpret_cast<char*>(m_rangeInfo.mapPtr) + m_allocationOffset;
      result.gpuAddress = m_rangeInfo.gpuAddress + m_allocationOffset;

      m_allocationOffset += size;
      return result;
    }

  private:

    DxvkResourceDescriptorHeap* m_heap = nullptr;

    std::atomic<uint32_t>   m_useCount = { 0u };

    Rc<DxvkBuffer>          m_gpuBuffer = nullptr;

    VkDeviceSize            m_rangeOffset = 0u;
    VkDeviceSize            m_rangeSize   = 0u;

    VkDeviceSize            m_allocationOffset = 0u;

    VkDeviceSize            m_reservedSize = 0u;
    VkDeviceSize            m_bufferSize = 0u;

    DxvkResourceBufferInfo  m_rangeInfo = { };

    void reset() {
      m_allocationOffset = 0u;
    }

  };


  /**
   * \brief Resource descriptor heap
   *
   * Manages descriptor memory for view and buffer descriptors.
   */
  class DxvkResourceDescriptorHeap {

  public:

    DxvkResourceDescriptorHeap(DxvkDevice* device);

    ~DxvkResourceDescriptorHeap();

    /**
     * \brief Increments ref count
     */
    void incRef() {
      m_useCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements ref count
     * Frees object when the last reference is removed.
     */
    void decRef() {
      if (m_useCount.fetch_sub(1u, std::memory_order_release) == 1u)
        delete this;
    }

    /**
     * \brief Retrieves current descriptor range
     *
     * This will always be the most recently allocated range.
     * It is not guaranteed to be empty or be able to service
     * any allocations.
     * \returns Current descriptor range
     */
    Rc<DxvkResourceDescriptorRange> getRange() {
      if (unlikely(!m_currentRange))
        m_currentRange = addRanges();

      return m_currentRange;
    }

    /**
     * \brief Allocates a new descriptor range
     *
     * Returns an empty and unused descriptor range. Subsequent
     * calls to \c getRange will return the same range.
     * If the base address of the underlying descriptor heap
     * changes, it must be bound to the command list.
     * \returns Newly allocated descriptor range
     */
    Rc<DxvkResourceDescriptorRange> allocRange();

  private:

    DxvkDevice*           m_device    = nullptr;
    std::atomic<uint32_t> m_useCount  = { 0u };

    VkDeviceSize          m_reservedSize = 0u;

    std::list<DxvkResourceDescriptorRange> m_ranges;

    DxvkResourceDescriptorRange* m_currentRange = nullptr;

    DxvkResourceDescriptorRange* addRanges();

    Rc<DxvkBuffer> createBuffer(VkDeviceSize baseSize);

  };



  inline void DxvkResourceDescriptorRange::incRef() {
    if (m_useCount.fetch_add(1u, std::memory_order_acquire) == 0u)
      m_heap->incRef();
  }


  inline void DxvkResourceDescriptorRange::decRef() {
    if (m_useCount.fetch_sub(1u, std::memory_order_release) == 1u)
      m_heap->decRef();
  }

}
