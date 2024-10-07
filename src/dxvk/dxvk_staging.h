#pragma once

#include <queue>

#include "dxvk_buffer.h"
#include "dxvk_device.h"

namespace dxvk {

  /**
   * \brief Staging buffer statistics
   *
   * Can optionally be used to throttle resource
   * uploads through the staging buffer.
   */
  struct DxvkStagingBufferStats {
    /// Total amount allocated since the buffer was created
    VkDeviceSize allocatedTotal = 0u;
    /// Amount allocated since the last time the buffer was reset
    VkDeviceSize allocatedSinceLastReset = 0u;
  };


  /**
   * \brief Staging buffer
   *
   * Provides a simple linear staging buffer
   * allocator for data uploads.
   */
  class DxvkStagingBuffer {

  public:

    /**
     * \brief Creates staging buffer
     *
     * \param [in] device DXVK device
     * \param [in] size Buffer size
     */
    DxvkStagingBuffer(
      const Rc<DxvkDevice>&     device,
            VkDeviceSize        size);

    /**
     * \brief Frees staging buffer
     */
    ~DxvkStagingBuffer();

    /**
     * \brief Allocates staging buffer memory
     *
     * Tries to suballocate from existing buffer,
     * or creates a new buffer if necessary.
     * \param [in] size Number of bytes to allocate
     * \returns Allocated slice
     */
    DxvkBufferSlice alloc(VkDeviceSize size);

    /**
     * \brief Resets staging buffer and allocator
     */
    void reset();

    /**
     * \brief Retrieves allocation statistics
     * \returns Current allocation statistics
     */
    DxvkStagingBufferStats getStatistics() const {
      DxvkStagingBufferStats result = { };
      result.allocatedTotal = m_allocationCounter;
      result.allocatedSinceLastReset = m_allocationCounter - m_allocationCounterValueOnReset;
      return result;
    }

  private:

    Rc<DxvkDevice>  m_device = nullptr;
    Rc<DxvkBuffer>  m_buffer = nullptr;
    VkDeviceSize    m_offset = 0u;
    VkDeviceSize    m_size = 0u;

    VkDeviceSize    m_allocationCounter = 0u;
    VkDeviceSize    m_allocationCounterValueOnReset = 0u;

  };

}
