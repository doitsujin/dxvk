#include "d3d9_mem.h"
#include "../util/util_string.h"
#include "../util/util_math.h"
#include "../util/log/log.h"
#include "../util/util_likely.h"
#include <utility>
#include <algorithm>

#ifdef D3D9_ALLOW_UNMAPPING
#include <sysinfoapi.h>
#else
#include <stdlib.h>
#endif

namespace dxvk {

#ifdef D3D9_ALLOW_UNMAPPING
  D3D9MemoryAllocator::D3D9MemoryAllocator() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_allocationGranularity = sysInfo.dwAllocationGranularity;
    m_mappingGranularity = m_allocationGranularity * 16;
  }

  D3D9Memory D3D9MemoryAllocator::Alloc(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    uint32_t alignedSize = align(Size, CACHE_LINE_SIZE);
    for (auto& chunk : m_chunks) {
      D3D9Memory memory = chunk->AllocLocked(alignedSize);
      if (memory) {
        m_usedMemory += memory.GetSize();
        return memory;
      }
    }

    uint32_t chunkSize = std::max(D3D9ChunkSize, alignedSize);
    m_allocatedMemory += chunkSize;

    D3D9MemoryChunk* chunk = new D3D9MemoryChunk(this, chunkSize);
    std::unique_ptr<D3D9MemoryChunk> uniqueChunk(chunk);
    D3D9Memory memory = uniqueChunk->AllocLocked(alignedSize);
    m_usedMemory += memory.GetSize();

    m_chunks.push_back(std::move(uniqueChunk));
    return memory;
  }

  void D3D9MemoryAllocator::Free(D3D9Memory *Memory) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    D3D9MemoryChunk* chunk = Memory->GetChunk();
    chunk->FreeLocked(Memory);
    m_usedMemory -= Memory->GetSize();
    if (chunk->IsEmpty())
      FreeChunk(chunk);
  }

  void D3D9MemoryAllocator::FreeChunk(D3D9MemoryChunk *Chunk) {
    // Has to be called in the lock

    m_allocatedMemory -= Chunk->Size();

    m_chunks.erase(std::remove_if(m_chunks.begin(), m_chunks.end(), [&](auto& item) {
        return item.get() == Chunk;
    }), m_chunks.end());
  }

  void* D3D9MemoryAllocator::Map(D3D9Memory* Memory) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    D3D9MemoryChunk* chunk = Memory->GetChunk();
    uint32_t memoryMapped;
    void* ptr = chunk->MapLocked(Memory, memoryMapped);
    m_mappedMemory += memoryMapped;
    return ptr;
  }

  void D3D9MemoryAllocator::Unmap(D3D9Memory* Memory) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    D3D9MemoryChunk* chunk = Memory->GetChunk();
    m_mappedMemory -= chunk->UnmapLocked(Memory);
  }

  uint32_t D3D9MemoryAllocator::MappedMemory() const {
    return m_mappedMemory.load();
  }

  uint32_t D3D9MemoryAllocator::UsedMemory() const {
    return m_usedMemory.load();
  }

  uint32_t D3D9MemoryAllocator::AllocatedMemory() const {
    return m_allocatedMemory.load();
  }

  D3D9MemoryChunk::D3D9MemoryChunk(D3D9MemoryAllocator* Allocator, uint32_t Size)
    : m_allocator(Allocator), m_size(Size) {
    m_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, 0, Size, nullptr);
    m_freeRanges.push_back({ 0, Size });
    uint32_t mappingGranularity = Allocator->MappingGranularity();
    m_mappingRanges.resize(((Size + mappingGranularity - 1) / mappingGranularity));
  }

  D3D9MemoryChunk::~D3D9MemoryChunk() {
    // Has to be protected by the allocator lock

    CloseHandle(m_mapping);
  }

  void* D3D9MemoryChunk::MapLocked(D3D9Memory* Memory, uint32_t& mappedSize) {
    // Has to be protected by the allocator lock

    mappedSize = 0;
    uint32_t mappingGranularity = m_allocator->MappingGranularity();

    uint32_t alignedOffset = alignDown(Memory->GetOffset(), mappingGranularity);
    uint32_t alignmentDelta = Memory->GetOffset() - alignedOffset;
    uint32_t alignedSize = Memory->GetSize() + alignmentDelta;
    if (alignedSize > mappingGranularity) {
      // The allocation crosses the boundary of the internal mapping page it's a part of
      // so we map it on it's own.
      alignedOffset = alignDown(Memory->GetOffset(), m_allocator->AllocationGranularity());
      alignmentDelta = Memory->GetOffset() - alignedOffset;
      alignedSize = Memory->GetSize() + alignmentDelta;

      mappedSize = alignedSize;
      uint8_t* basePtr = static_cast<uint8_t*>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, alignedOffset, alignedSize));
      if (unlikely(basePtr == nullptr)) {
        DWORD error = GetLastError();
        Logger::err(str::format("Mapping non-persisted file failed: ", error, ", Mapped memory: ", m_allocator->MappedMemory()));
        return nullptr;
      }
      return basePtr + alignmentDelta;
    }

    // For small allocations we map the entire mapping page to minimize the overhead from having the align the offset to 65k bytes.
    // This should hopefully also reduce the amount of MapViewOfFile calls we do for tiny allocations.
    auto& mappingRange = m_mappingRanges[Memory->GetOffset() / mappingGranularity];
    if (unlikely(mappingRange.refCount == 0)) {
      mappedSize = mappingGranularity;
      mappingRange.ptr = static_cast<uint8_t*>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, alignedOffset, m_allocator->MappingGranularity()));
      if (unlikely(mappingRange.ptr == nullptr)) {
        DWORD error = GetLastError();
        LPTSTR buffer = nullptr;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPTSTR)&buffer, 0, nullptr);
        Logger::err(str::format("Mapping non-persisted file failed: ", error, ", Mapped memory: ", m_allocator->MappedMemory(), ", Msg: ", buffer));
        if (buffer) {
          LocalFree(buffer);
        }
      }
    }
    mappingRange.refCount++;
    uint8_t* basePtr = static_cast<uint8_t*>(mappingRange.ptr);
    return basePtr + alignmentDelta;
  }

  uint32_t D3D9MemoryChunk::UnmapLocked(D3D9Memory* Memory) {
    // Has to be protected by the allocator lock

    uint32_t mappingGranularity = m_allocator->MappingGranularity();

    uint32_t alignedOffset = alignDown(Memory->GetOffset(), mappingGranularity);
    uint32_t alignmentDelta = Memory->GetOffset() - alignedOffset;
    uint32_t alignedSize = Memory->GetSize() + alignmentDelta;
    if (alignedSize > mappingGranularity) {
      // Single use mapping
      alignedOffset = alignDown(Memory->GetOffset(), m_allocator->AllocationGranularity());
      alignmentDelta = Memory->GetOffset() - alignedOffset;
      alignedSize = Memory->GetSize() + alignmentDelta;

      uint8_t* basePtr = static_cast<uint8_t*>(Memory->Ptr()) - alignmentDelta;
      UnmapViewOfFile(basePtr);
      return alignedSize;
    }
    auto& mappingRange = m_mappingRanges[Memory->GetOffset() / mappingGranularity];
    mappingRange.refCount--;
    if (unlikely(mappingRange.refCount == 0)) {
      UnmapViewOfFile(mappingRange.ptr);
      mappingRange.ptr = nullptr;
      return mappingGranularity;
    }
    return 0;
  }

  D3D9Memory D3D9MemoryChunk::AllocLocked(uint32_t Size) {
    // Has to be protected by the allocator lock

    uint32_t offset = 0;
    uint32_t size = 0;

    for (auto range = m_freeRanges.begin(); range != m_freeRanges.end(); range++) {
      if (range->length >= Size) {
        offset = range->offset;
        size = Size;
        range->offset += Size;
        range->length -= Size;
        if (range->length < (4 << 10)) {
          size += range->length;
          m_freeRanges.erase(range);
        }
        break;
      }
    }

    if (size != 0)
      return D3D9Memory(this, offset, Size);

    return {};
  }

  void D3D9MemoryChunk::FreeLocked(D3D9Memory *Memory) {
    // Has to be protected by the allocator lock

    uint32_t offset = Memory->GetOffset();
    uint32_t size = Memory->GetSize();

    auto curr = m_freeRanges.begin();

    // shamelessly stolen from dxvk_memory.cpp
    while (curr != m_freeRanges.end()) {
      if (curr->offset == offset + size) {
        size += curr->length;
        curr = m_freeRanges.erase(curr);
      } else if (curr->offset + curr->length == offset) {
        offset -= curr->length;
        size += curr->length;
        curr = m_freeRanges.erase(curr);
      } else {
        curr++;
      }
    }

    m_freeRanges.push_back({ offset, size });
  }

  bool D3D9MemoryChunk::IsEmpty() const {
    // Has to be protected by the allocator lock

    return m_freeRanges.size() == 1
        && m_freeRanges[0].length == m_size;
  }

  D3D9MemoryAllocator* D3D9MemoryChunk::Allocator() const {
    return m_allocator;
  }


  D3D9Memory::D3D9Memory(D3D9MemoryChunk* Chunk, size_t Offset, size_t Size)
    : m_chunk(Chunk), m_offset(Offset), m_size(Size) {}

  D3D9Memory::D3D9Memory(D3D9Memory&& other)
    : m_chunk(std::exchange(other.m_chunk, nullptr)),
      m_ptr(std::exchange(other.m_ptr, nullptr)),
      m_offset(std::exchange(other.m_offset, 0)),
      m_size(std::exchange(other.m_size, 0)) {}

  D3D9Memory::~D3D9Memory() {
    this->Free();
  }

  D3D9Memory& D3D9Memory::operator = (D3D9Memory&& other) {
    this->Free();

    m_chunk = std::exchange(other.m_chunk, nullptr);
    m_ptr = std::exchange(other.m_ptr, nullptr);
    m_offset = std::exchange(other.m_offset, 0);
    m_size = std::exchange(other.m_size, 0);
    return *this;
  }

  void D3D9Memory::Free() {
    if (unlikely(m_chunk == nullptr))
      return;

    if (m_ptr != nullptr)
      Unmap();

    m_chunk->Allocator()->Free(this);
    m_chunk = nullptr;
  }

  void D3D9Memory::Map() {
    if (unlikely(m_ptr != nullptr))
      return;

    if (unlikely(m_chunk == nullptr))
      return;

    m_ptr = m_chunk->Allocator()->Map(this);
  }

  void D3D9Memory::Unmap() {
    if (unlikely(m_ptr == nullptr))
      return;

    m_chunk->Allocator()->Unmap(this);
    m_ptr = nullptr;
  }

  void* D3D9Memory::Ptr() {
    return m_ptr;
  }

#else

  D3D9Memory D3D9MemoryAllocator::Alloc(uint32_t Size) {
    D3D9Memory memory(this, Size);
    m_allocatedMemory += Size;
    return memory;
  }

  uint32_t D3D9MemoryAllocator::MappedMemory() const {
    return m_allocatedMemory.load();
  }

  uint32_t D3D9MemoryAllocator::UsedMemory() const {
    return m_allocatedMemory.load();
  }

  uint32_t D3D9MemoryAllocator::AllocatedMemory() const {
    return m_allocatedMemory.load();
  }

  D3D9Memory::D3D9Memory(D3D9MemoryAllocator* pAllocator, size_t Size)
    : m_allocator (pAllocator),
      m_ptr       (malloc(Size)),
      m_size      (Size) {}

  D3D9Memory::D3D9Memory(D3D9Memory&& other)
    : m_allocator(std::exchange(other.m_allocator, nullptr)),
      m_ptr(std::exchange(other.m_ptr, nullptr)),
      m_size(std::exchange(other.m_size, 0)) {}

  D3D9Memory::~D3D9Memory() {
    this->Free();
  }

  D3D9Memory& D3D9Memory::operator = (D3D9Memory&& other) {
    this->Free();

    m_allocator = std::exchange(other.m_allocator, nullptr);
    m_ptr = std::exchange(other.m_ptr, nullptr);
    m_size = std::exchange(other.m_size, 0);
    return *this;
  }

  void D3D9Memory::Free() {
    if (m_ptr == nullptr)
      return;

    free(m_ptr);
    m_ptr = nullptr;
    m_allocator->NotifyFreed(m_size);
  }


#endif

}
