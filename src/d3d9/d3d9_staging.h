#pragma once

#include "../dxvk/dxvk_staging.h"
#include "../dxvk/dxvk_marker.h"

#include "d3d9_include.h"

namespace dxvk {

  class D3D9DeviceEx;

  struct D3D9StagingAlloc {
    Rc<DxvkMarker> marker;
    uint64_t sequenceNumber;
    uint32_t size;

    D3D9StagingAlloc()
      : marker(nullptr),
        sequenceNumber(0),
        size(0) {}

    D3D9StagingAlloc(Rc<DxvkMarker> Marker, uint64_t SequenceNumber, uint32_t Size)
      : marker(Marker),
        sequenceNumber(SequenceNumber),
        size(Size) {}
  };

  class D3D9StagingBuffer {

    constexpr static VkDeviceSize StagingBufferSize = 4ull << 20;

  public:

    D3D9StagingBuffer(D3D9DeviceEx* pDevice);

    DxvkBufferSlice Alloc(uint32_t Size);

    uint32_t StagingMemory() const {
      return m_stagingMem.load();
    }

  private:

    D3D9DeviceEx* m_device;

    DxvkStagingBuffer m_stagingBuffer;

    std::atomic<uint32_t>           m_stagingMem = 0;
    std::list<D3D9StagingAlloc>     m_stagingAllocs;
  };

}
