#include "d3d9_mem.h"
#include "../util/util_string.h"
#include "../util/log/log.h"
#include "../util/util_math.h"
#include "../util/util_env.h"
#include "d3d9_include.h"
#include <utility>

namespace dxvk {

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

}
