
#pragma once

#include "../util/thread.h"

#if defined(_WIN32) && !defined(_WIN64)
  #define D3D9_ALLOW_UNMAPPING
#endif

#ifdef D3D9_ALLOW_UNMAPPING
  #define WIN32_LEAN_AND_MEAN
  #include <winbase.h>
#endif

namespace dxvk {

  class D3D9MemoryAllocator;
  class D3D9Memory;

#ifdef D3D9_ALLOW_UNMAPPING

  class D3D9MemoryChunk;

  constexpr uint32_t D3D9ChunkSize = 16 << 20;

  struct D3D9MemoryRange {
    uint32_t offset;
    uint32_t length;
  };

  class D3D9MemoryChunk {
    friend D3D9MemoryAllocator;

    public:
      ~D3D9MemoryChunk();

      D3D9MemoryChunk             (const D3D9MemoryChunk&) = delete;
      D3D9MemoryChunk& operator = (const D3D9MemoryChunk&) = delete;

      D3D9MemoryChunk             (D3D9MemoryChunk&& other) = delete;
      D3D9MemoryChunk& operator = (D3D9MemoryChunk&& other) = delete;

      void IncMapCounter();
      void DecMapCounter();
      void* Ptr() const { return m_ptr; }
      D3D9Memory Alloc(uint32_t Size);
      void Free(D3D9Memory* Memory);
      bool IsEmpty();
      uint32_t Size() const { return m_size; }
      D3D9MemoryAllocator* Allocator() const;

    private:
      D3D9MemoryChunk(D3D9MemoryAllocator* Allocator, uint32_t Size);

      dxvk::mutex m_mutex;
      D3D9MemoryAllocator* m_allocator;
      uint32_t m_mapCounter = 0;
      HANDLE m_mapping;
      void* m_ptr;
      uint32_t m_size;
      std::vector<D3D9MemoryRange> m_freeRanges;
  };

  class D3D9Memory {
    friend D3D9MemoryChunk;

    public:
      D3D9Memory() {}
      ~D3D9Memory();

      D3D9Memory             (const D3D9Memory&) = delete;
      D3D9Memory& operator = (const D3D9Memory&) = delete;

      D3D9Memory             (D3D9Memory&& other);
      D3D9Memory& operator = (D3D9Memory&& other);

      operator bool() const { return m_chunk != nullptr; }

      void Map();
      void Unmap();
      void* Ptr();
      D3D9MemoryChunk* GetChunk() const { return m_chunk; }
      size_t GetOffset() const { return m_offset; }
      size_t GetSize() const { return m_size; }

    private:
      D3D9Memory(D3D9MemoryChunk* Chunk, size_t Offset, size_t Size);
      void Free();

      D3D9MemoryChunk* m_chunk = nullptr;
      void* m_ptr              = nullptr;
      size_t m_offset          = 0;
      size_t m_size            = 0;
  };

  class D3D9MemoryAllocator {
    friend D3D9MemoryChunk;

    public:
      D3D9MemoryAllocator() = default;
      ~D3D9MemoryAllocator() = default;
      D3D9Memory Alloc(uint32_t Size);
      void FreeChunk(D3D9MemoryChunk* Chunk);
      void NotifyMapped(uint32_t Size);
      void NotifyUnmapped(uint32_t Size);
      void NotifyFreed(uint32_t Size);
      uint32_t MappedMemory();
      uint32_t UsedMemory();
      uint32_t AllocatedMemory();

    private:
      dxvk::mutex m_mutex;
      std::vector<std::unique_ptr<D3D9MemoryChunk>> m_chunks;
      size_t m_mappedMemory = 0;
      size_t m_allocatedMemory = 0;
      size_t m_usedMemory = 0;
  };

#else
    class D3D9Memory {
    public:
      D3D9Memory() = default;
      D3D9Memory(D3D9MemoryAllocator* Allocator, size_t Size);
      ~D3D9Memory();

      D3D9Memory             (const D3D9Memory&) = delete;
      D3D9Memory& operator = (const D3D9Memory&) = delete;

      D3D9Memory             (D3D9Memory&& other);
      D3D9Memory& operator = (D3D9Memory&& other);

      operator bool() const { return m_ptr != nullptr; }

      void Map(){}
      void Unmap(){}
      void* Ptr() { return m_ptr; }

    private:
      void Free();

      D3D9MemoryAllocator* m_allocator = nullptr;
      void* m_ptr              = nullptr;
      size_t m_offset          = 0;
      size_t m_size            = 0;
    };

    class D3D9MemoryAllocator {

    public:
      D3D9MemoryAllocator() = default;
      ~D3D9MemoryAllocator() = default;
      D3D9Memory Alloc(uint32_t Size);
      void NotifyFreed(uint32_t Size);
      uint32_t MappedMemory();
      uint32_t UsedMemory();
      uint32_t AllocatedMemory();

    private:
      dxvk::mutex m_mutex;
      size_t m_usedMemory = 0;
    };

#endif

}
