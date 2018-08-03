#pragma once

#include "d3d11_include.h"

namespace dxvk {

  class D3D11Device;

  /**
   * \brief UAV counter structure
   * 
   * Data structure passed to shaders that use
   * append/consume buffer functionality.
   */
  struct D3D11UavCounter {
    uint32_t atomicCtr;
  };


  /**
   * \brief D3D11 UAV counter slice allocator
   * 
   * Thread-safe allocator for UAV counter slices.
   * The resulting slices are aligned to the device's
   * \c minStorageBufferOffsetAlignment.
   */
  class D3D11UavCounterAllocator {
    constexpr static VkDeviceSize SlicesPerBuffer = 16384;
  public:

    D3D11UavCounterAllocator(
            D3D11Device*          pDevice);
    
    ~D3D11UavCounterAllocator();

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

    D3D11Device*                  m_device;
    VkDeviceSize                  m_alignment;

    std::mutex                    m_mutex;
    std::vector<DxvkBufferSlice>  m_freeSlices;

    void CreateBuffer(VkDeviceSize SliceCount);

    VkDeviceSize GetOffsetAlignment() const;

  };

}