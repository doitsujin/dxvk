#include "d3d9_staging.h"
#include "d3d9_device.h"

namespace dxvk {

  D3D9StagingBuffer::D3D9StagingBuffer(D3D9DeviceEx* pDevice)
  : m_device        (pDevice),
    m_stagingBuffer ( pDevice->GetDXVKDevice(), StagingBufferSize ) {
  }

  DxvkBufferSlice D3D9StagingBuffer::Alloc(uint32_t Size) {
    const uint32_t limit = m_device->GetOptions()->stagingMemory;
    if (env::is32BitHostPlatform() && limit != 0) {
      uint64_t sequenceNumber = m_device->GetCurrentSequenceNumber();
      D3D9StagingAlloc last;
      bool pastFinishedAllocations = false;
      for (auto iter = m_stagingAllocs.cbegin(); iter != m_stagingAllocs.cend();) {
        if (!pastFinishedAllocations && sequenceNumber > iter->sequenceNumber && !iter->marker->isInUse(DxvkAccess::Write)) {
          // The memory used for this allocation has already been reclaimed.
          m_stagingMem -= iter->size;
          iter = m_stagingAllocs.erase(iter);
          continue;
        }

        // The list is ordered, so once we've reached the first entry that is still in use,
        // we don't have to check the entries that come afterwards.
        pastFinishedAllocations = true;

        if (m_stagingMem >= uint32_t(limit)) {
          // We're past the limit, find the newest entry we need to wait for to get under the limit again.
          m_stagingMem -= iter->size;
          last = *iter;
          iter = m_stagingAllocs.erase(iter);
          continue;
        }

        break;
      }

      if (last.marker != nullptr) {
        // This should hopefully only happen on loading screens.
        // Either way, stalling is preferable to crashing.
        Logger::warn("Staging memory exhausted. Stalling");
        m_device->WaitForResource(last.marker, last.sequenceNumber, 0);
      }

      uint32_t alignedSize = dxvk::align(Size, 256);
      m_stagingMem += alignedSize;
      if (!m_stagingAllocs.empty() && m_stagingAllocs.back().sequenceNumber == sequenceNumber) {
        m_stagingAllocs.back().size += alignedSize;
      } else {
        Rc<DxvkMarker> marker = m_device->GetDXVKDevice()->createMarker();
        m_stagingAllocs.push_back(D3D9StagingAlloc(marker, sequenceNumber, alignedSize));

        m_device->EmitCs([
          cMarker = std::move(marker)
        ] (DxvkContext* ctx) {
          ctx->insertMarker(cMarker);
        });
      }
    }

    return m_stagingBuffer.alloc(256, Size);
  }
}
