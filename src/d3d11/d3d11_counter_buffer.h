#pragma once

#include "d3d11_include.h"

#include "../dxvk/dxvk_buffer.h"
#include "../dxvk/dxvk_device.h"

namespace dxvk {

  class D3D11Device;

  /**
   * \brief D3D11 UAV counter slice allocator
   * 
   * Thread safe allocator for buffer slices of
   * the same size, which are typically used to
   * store counters (such as UAV counters).
   */
  class D3D11CounterBuffer : public RcObject {

  public:

    D3D11CounterBuffer(
      const Rc<DxvkDevice>&       Device,
      const DxvkBufferCreateInfo& BufferInfo,
            VkDeviceSize          SliceLength);
    
    ~D3D11CounterBuffer();

    /**
     * \brief Allocates a counter slice
     * 
     * Picks a slice from the free list or
     * creates a new buffer if necessary.
     * \returns The counter slice
     */
    DxvkBufferSlice AllocSlice();

    /**
     * \brief Frees a counter slice
     * 
     * Adds the given slice back to the
     * free list so that it can be reused.
     * \param [in] Slice the slice to free
     */
    void FreeSlice(
      const DxvkBufferSlice&      Slice);

  private:

    Rc<DxvkDevice>                m_device;

    DxvkBufferCreateInfo          m_bufferInfo;
    VkDeviceSize                  m_sliceLength;

    std::mutex                    m_mutex;
    std::vector<DxvkBufferSlice>  m_freeSlices;

    void CreateBuffer();

  };

}