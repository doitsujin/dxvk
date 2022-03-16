#include "d3d9_mem.h"
#include "../util/util_string.h"
#include "../util/util_math.h"
#include "../util/log/log.h"
#include "../util/util_likely.h"
#include <utility>

namespace dxvk {

#ifdef D3D9_ALLOW_UNMAPPING
  D3D9Memory D3D9MemoryAllocator::Alloc(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    uint32_t alignedSize = align(Size, 16);
    for (auto& chunk : m_chunks) {
      D3D9Memory memory = chunk->Alloc(alignedSize);
      if (memory) {
        m_usedMemory += memory.GetSize();
        return memory;
      }
    }

    alignedSize = align(Size, 4 << 10);
    uint32_t chunkSize = std::max(D3D9ChunkSize, alignedSize);
    m_allocatedMemory += chunkSize;

    D3D9MemoryChunk* chunk = new D3D9MemoryChunk(this, chunkSize);
    std::unique_ptr<D3D9MemoryChunk> uniqueChunk(chunk);
    D3D9Memory memory = uniqueChunk->Alloc(alignedSize);
    m_usedMemory += memory.GetSize();

    m_chunks.push_back(std::move(uniqueChunk));
    return memory;
  }

  void D3D9MemoryAllocator::FreeChunk(D3D9MemoryChunk *Chunk) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_allocatedMemory -= Chunk->Size();

    m_chunks.erase(std::remove_if(m_chunks.begin(), m_chunks.end(), [&](auto& item) {
        return item.get() == Chunk;
    }), m_chunks.end());
  }

  void D3D9MemoryAllocator::NotifyMapped(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_mappedMemory += Size;
  }

  void D3D9MemoryAllocator::NotifyUnmapped(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_mappedMemory -= Size;
  }

  void D3D9MemoryAllocator::NotifyFreed(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_usedMemory -= Size;
  }

  uint32_t D3D9MemoryAllocator::MappedMemory() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    return m_mappedMemory;
  }

  uint32_t D3D9MemoryAllocator::UsedMemory() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    return m_usedMemory;
  }

  uint32_t D3D9MemoryAllocator::AllocatedMemory() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    return m_allocatedMemory;
  }


  D3D9MemoryChunk::D3D9MemoryChunk(D3D9MemoryAllocator* Allocator, uint32_t Size)
    : m_allocator(Allocator), m_size(Size) {
    m_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, 0, Size, nullptr);
    m_freeRanges.push_back({ 0, Size });
  }

  D3D9MemoryChunk::~D3D9MemoryChunk() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_ptr != nullptr) {
      UnmapViewOfFile(m_ptr);
    }
    CloseHandle(m_mapping);
  }

  void D3D9MemoryChunk::IncMapCounter() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_mapCounter++;

    if (m_mapCounter == 1) {
      m_allocator->NotifyMapped(m_size);
      m_ptr = MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, m_size);
    }
  }

  void D3D9MemoryChunk::DecMapCounter() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_mapCounter--;

    if (m_mapCounter == 0) {
      m_allocator->NotifyUnmapped(m_size);
      UnmapViewOfFile(m_ptr);
      m_ptr = nullptr;
    }
  }

  D3D9Memory D3D9MemoryChunk::Alloc(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

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
      return D3D9Memory(this, offset, size);

    return {};
  }

  void D3D9MemoryChunk::Free(D3D9Memory *Memory) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    uint32_t offset = Memory->GetOffset();
    uint32_t length = Memory->GetSize();

    auto curr = m_freeRanges.begin();

    // shamelessly stolen from dxvk_memory.cpp
    while (curr != m_freeRanges.end()) {
      if (curr->offset == offset + length) {
        length += curr->length;
        curr = m_freeRanges.erase(curr);
      } else if (curr->offset + curr->length == offset) {
        offset -= curr->length;
        length += curr->length;
        curr = m_freeRanges.erase(curr);
      } else {
        curr++;
      }
    }

    m_freeRanges.push_back({ offset, length });
    m_allocator->NotifyFreed(Memory->GetSize());
  }

  bool D3D9MemoryChunk::IsEmpty() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

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
      m_chunk->DecMapCounter();

    m_chunk->Free(this);
    if (m_chunk->IsEmpty()) {
      D3D9MemoryAllocator* allocator = m_chunk->Allocator();
      allocator->FreeChunk(m_chunk);
    }
    m_chunk = nullptr;
  }

  void D3D9Memory::Map() {
    if (unlikely(m_ptr != nullptr))
      return;

    if (unlikely(m_chunk == nullptr)) {
      Logger::err("Tried to map dead memory");
      return;
    }

    m_chunk->IncMapCounter();
    uint8_t* ptr = reinterpret_cast<uint8_t*>(m_chunk->Ptr());
    m_ptr = ptr + m_offset;
  }

  void D3D9Memory::Unmap() {
    if (unlikely(m_ptr == nullptr))
      return;

    m_ptr = nullptr;
    m_chunk->DecMapCounter();
  }

  void* D3D9Memory::Ptr() {
    if (unlikely(m_ptr == nullptr)) {
      Logger::err("Tried accessing unmapped memory.");
      return nullptr;
    }

    return m_ptr;
  }

#else

    D3D9Memory D3D9MemoryAllocator::Alloc(uint32_t Size) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      m_usedMemory += Size;

      return D3D9Memory(this, Size);
    }

    void D3D9MemoryAllocator::NotifyFreed(uint32_t Size) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      m_usedMemory -= Size;
    }

    uint32_t D3D9MemoryAllocator::MappedMemory() {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      return m_usedMemory;
    }

    uint32_t D3D9MemoryAllocator::UsedMemory() {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      return m_usedMemory;
    }

    uint32_t D3D9MemoryAllocator::AllocatedMemory() {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      return m_usedMemory;
    }

    D3D9Memory::D3D9Memory(D3D9MemoryAllocator* Allocator, size_t Size)
      : m_allocator(Allocator), m_ptr(std::malloc(Size)), m_size(Size) { }

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
      if (!m_ptr)
        return;

      m_allocator->NotifyFreed(m_size);

      std::free(m_ptr);
    }

#endif

}
