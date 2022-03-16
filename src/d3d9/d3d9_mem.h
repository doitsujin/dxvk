
#pragma once

#include "../util/thread.h"

namespace dxvk {

  class D3D9MemoryAllocator;
  class D3D9Memory;
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

}
