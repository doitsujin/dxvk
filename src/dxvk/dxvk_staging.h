#pragma once

#include <queue>

#include "dxvk_buffer.h"

namespace dxvk {
  
  class DxvkDevice;

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
     * \param [in] align Minimum alignment
     * \param [in] size Number of bytes to allocate
     * \returns Allocated slice
     */
    DxvkBufferSlice alloc(VkDeviceSize align, VkDeviceSize size);

    /**
     * \brief Resets staging buffer and allocator
     */
    void reset();

  private:

    Rc<DxvkDevice>  m_device;
    Rc<DxvkBuffer>  m_buffer;
    VkDeviceSize    m_offset;
    VkDeviceSize    m_size;

  };

}
