#include "util_unmap.h"

#include "./log/log.h"

#ifdef _WIN32
#include "./com/com_include.h"
#endif

namespace dxvk {

#ifdef DXVK_USE_UNMAPPABLE_MEMORY
  MemoryFile::MemoryFile(size_t size) {
    create(size);
  }


  MemoryFile::MemoryFile(MemoryFile&& other)
  : m_data(std::exchange(other.m_data, Data())) { }


  MemoryFile& MemoryFile::operator = (MemoryFile&& other) {
    destroy();

    m_data = std::exchange(other.m_data, Data());
    return *this;
  }


  MemoryFile::~MemoryFile() {
    destroy();
  }


  size_t MemoryFile::granularity() const {
    static std::atomic<size_t> s_granularity = { 0u };

    size_t result = s_granularity.load(std::memory_order_relaxed);

    if (result)
      return result;

    SYSTEM_INFO sysInfo = { };
    GetSystemInfo(&sysInfo);

    // Bump granularity to the allocator's page size for consistenty
    result = sysInfo.dwAllocationGranularity;
    result = std::max(result, uint32_t(DxvkPageAllocator::PageSize));

    s_granularity.store(result, std::memory_order_relaxed);
    return result;
  }


  void* MemoryFile::map(size_t offset, size_t size) {
    return MapViewOfFile(m_data.mapping, FILE_MAP_ALL_ACCESS, 0, offset, size);
  }


  void MemoryFile::unmap(void* ptr) {
    UnmapViewOfFile(ptr);
  }


  void MemoryFile::destroy() {
    if (!m_data.size)
      return;

    CloseHandle(m_data.mapping);
  }


  void MemoryFile::create(size_t size) {
    m_data.mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE | SEC_COMMIT, 0, size, nullptr);
    m_data.size = size;

    if (!m_data.mapping)
      throw DxvkError("Failed to create file mapping.");
  }




  MemoryFilePool::MemoryFilePool()
  : m_poolAllocator(m_pageAllocator) {

  }


  MemoryFilePool::~MemoryFilePool() {

  }


  int32_t MemoryFilePool::alloc(size_t size) {
    std::lock_guard lock(m_mutex);

    int32_t index;

    if (!m_freeList.empty()) {
      index = m_freeList.back();
      m_freeList.pop_back();
    } else {
      index = int32_t(m_allocations.size());
      m_allocations.emplace_back();
    }

    auto& allocInfo = m_allocations.at(index);

    if (size <= DxvkPageAllocator::MaxChunkSize) {
      auto address = (size <= DxvkPoolAllocator::MaxSize)
        ? m_poolAllocator.alloc(size)
        : m_pageAllocator.alloc(size, 1u);

      if (address < 0) {
        addChunk(size);

        address = (size <= DxvkPoolAllocator::MaxSize)
          ? m_poolAllocator.alloc(size)
          : m_pageAllocator.alloc(size, 1u);
      }

      if (address < 0) {
        m_freeList.push_back(index);
        return -1;
      }

      allocInfo.address = address;
      allocInfo.size = size;
    } else {
      allocInfo.file = MemoryFile(size);
      allocInfo.size = size;

      m_memoryAllocated += size;
    }

    m_memoryUsed += size;
    return index;
  }


  void MemoryFilePool::free(int32_t allocation) {
    std::lock_guard lock(m_mutex);

    auto& allocInfo = m_allocations.at(allocation);
    m_memoryUsed -= allocInfo.size;

    if (allocInfo.size <= DxvkPageAllocator::MaxChunkSize) {
      auto chunkIndex = allocInfo.address >> DxvkPageAllocator::ChunkAddressBits;

      if (allocInfo.size <= DxvkPoolAllocator::MaxSize)
        m_poolAllocator.free(allocInfo.address, allocInfo.size);
      else
        m_pageAllocator.free(allocInfo.address, allocInfo.size);

      if (!m_pageAllocator.pagesUsed(chunkIndex))
        freeChunk(chunkIndex);
    } else {
      m_memoryAllocated -= allocInfo.size;
    }

    allocInfo = AllocInfo();

    m_freeList.push_back(allocation);
  }


  void* MemoryFilePool::map(int32_t allocation) {
    std::lock_guard lock(m_mutex);

    auto& allocInfo = m_allocations.at(allocation);

    if (!(allocInfo.mapCount++)) {
      m_memoryMapped += allocInfo.size;

      if (allocInfo.file) {
        // Map entire file if the allocation owns it
        allocInfo.mapPtr = allocInfo.file.map(0u, allocInfo.size);
      } else {
        // Check whether the allocation is contained inside one page
        auto& chunk = m_chunks.at(allocInfo.address >> DxvkPageAllocator::ChunkAddressBits);
        auto chunkOffset = allocInfo.address & DxvkPageAllocator::ChunkAddressMask;

        auto pageSize = chunk.file.granularity();
        auto pageOffset = (chunkOffset % pageSize);

        auto pageLo = (chunkOffset / pageSize);
        auto pageHi = (chunkOffset + allocInfo.size - 1u) / pageSize;

        if (pageLo == pageHi) {
          if (chunk.pages.empty())
            chunk.pages.resize((chunk.size + pageSize - 1u) / pageSize);

          auto& page = chunk.pages.at(pageLo);

          if (!(page.mapCount++))
            page.mapPtr = chunk.file.map(chunkOffset - pageOffset, pageSize);

          allocInfo.mapPtr = reinterpret_cast<char*>(page.mapPtr) + pageOffset;
        } else {
          // We're straddling multiple pages, map them all and offset the base address
          auto mapPtr = chunk.file.map(pageLo * pageSize, (pageHi - pageLo + 1u) * pageSize);
          allocInfo.mapPtr = reinterpret_cast<char*>(mapPtr) + pageOffset;
        }
      }
    }

    return allocInfo.mapPtr;
  }


  void MemoryFilePool::unmap(int32_t allocation) {
    std::lock_guard lock(m_mutex);

    auto& allocInfo = m_allocations.at(allocation);

    if (!(--allocInfo.mapCount)) {
      m_memoryMapped -= allocInfo.size;

      if (allocInfo.file) {
        // Unmap entire file
        allocInfo.file.unmap(allocInfo.mapPtr);
      } else {
        // Check whether the allocation is contained inside one page
        auto& chunk = m_chunks.at(allocInfo.address >> DxvkPageAllocator::ChunkAddressBits);
        auto chunkOffset = allocInfo.address & DxvkPageAllocator::ChunkAddressMask;

        auto pageSize = chunk.file.granularity();
        auto pageOffset = (chunkOffset % pageSize);

        auto pageLo = (chunkOffset / pageSize);
        auto pageHi = (chunkOffset + allocInfo.size - 1u) / pageSize;

        if (pageLo == pageHi) {
          auto& page = chunk.pages.at(pageLo);

          if (!(--page.mapCount)) {
            chunk.file.unmap(page.mapPtr);
            page.mapPtr = nullptr;
          }
        } else {
          // Apply page offset to mapped pointer and unmap all pages
          auto basePtr = reinterpret_cast<char*>(allocInfo.mapPtr) - pageOffset;
          chunk.file.unmap(basePtr);
        }
      }

      allocInfo.mapPtr = nullptr;
    }
  }


  void MemoryFilePool::addChunk(size_t minSize) {
    auto chunkSize = DefaultChunkSize;

    while (chunkSize < minSize)
      chunkSize <<= 1u;

    auto chunkIndex = m_pageAllocator.addChunk(chunkSize);

    if (chunkIndex >= m_chunks.size())
      m_chunks.resize(chunkIndex + 1u);

    auto& chunk = m_chunks.at(chunkIndex);
    chunk.file = MemoryFile(chunkSize);
    chunk.size = chunkSize;

    m_memoryAllocated += chunk.size;
  }


  void MemoryFilePool::freeChunk(uint32_t index) {
    auto& chunk = m_chunks.at(index);
    m_memoryAllocated -= chunk.size;
    m_pageAllocator.removeChunk(index);
    chunk = ChunkInfo();
  }
#endif

}
