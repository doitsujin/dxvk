#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "../util/util_env.h"

namespace dxvk {

  /**
   * \brief Page allocator
   *
   * Implements a best-fit allocation strategy for coarse allocations
   * using an ordered free list. While allocating and freeing memory
   * are both linear in the worst case, minimum-size allocations can
   * generally be performed in constant time, with larger allocations
   * getting gradually slower.
   */
  class DxvkPageAllocator {

  public:

    /// Page size. While the allocator interface is fully designed around
    /// pages, defining a page size is useful for classes built on top of it.
    constexpr static uint32_t PageBits = 16;
    constexpr static uint64_t PageSize = 1u << PageBits;

    /// Maximum number of pages per chunk. Chunks represent contiguous memory
    /// allocations whose free regions can be merged.
    constexpr static uint32_t ChunkPageBits = 12u;
    constexpr static uint32_t ChunkPageMask = (1u << ChunkPageBits) - 1u;

    /// Chunk address bits. Can be used to quickly compute the chunk index
    /// and allocation offset within the chunk from a raw byte address.
    constexpr static uint32_t ChunkAddressBits = ChunkPageBits + PageBits;
    constexpr static uint64_t ChunkAddressMask = (1u << ChunkAddressBits) - 1u;

    constexpr static uint64_t MaxChunkSize = 1u << ChunkAddressBits;


    DxvkPageAllocator();

    ~DxvkPageAllocator();

    /**
     * \brief Queries total number of chunks
     *
     * This number may include chuks that have already been removed.
     * \returns Total chunk count
     */
    uint32_t chunkCount() const {
      return uint32_t(m_chunks.size());
    }

    /**
     * \brief Queries number of available pages in a chunk
     *
     * \param [in] chunkIndex Chunk index
     * \returns Capacity of the given chunk
     */
    uint32_t pageCount(uint32_t chunkIndex) const {
      return m_chunks.at(chunkIndex).pageCount;
    }

    /**
     * \brief Queries number of allocated pages in a chunk
     *
     * \param [in] chunkIndex Chunk index
     * \returns Used page count in the given chunk
     */
    uint32_t pagesUsed(uint32_t chunkIndex) const {
      return m_chunks.at(chunkIndex).pagesUsed;
    }

    /**
     * \brief Checks whether a chunk is alive
     *
     * \param [in] chunkIndex Chunk index
     * \returns \c true if chunk can be used
     */
    bool chunkIsAvailable(uint32_t chunkIndex) const {
      return !m_chunks.at(chunkIndex).disabled;
    }

    /**
     * \brief Allocates given number of bytes from the pool
     *
     * \param [in] size Allocation size, in bytes
     * \param [in] alignment Required aligment, in bytes
     * \returns Allocated byte address
     */
    int64_t alloc(uint64_t size, uint64_t alignment);

    /**
     * \brief Allocates pages
     *
     * \param [in] count Number of pages to allocate.
     *    Must be multiple of \c alignment.
     * \param [in] alignment Required alignment, in pages
     * \returns Page index, or -1 if not enough memory
     *    is available in the chunk.
     */
    int32_t allocPages(uint32_t count, uint32_t alignment);

    /**
     * \brief Frees allocated memory region
     *
     * \param [in] address Allocated address, in bytes
     * \param [in] size Allocation size, in bytes
     * \returns \c true if a chunk was freed
     */
    bool free(uint64_t address, uint64_t size);

    /**
     * \brief Frees pages
     *
     * \param [in] index Index of first page to free
     * \param [in] count Number of pages to free
     * \returns \c true if a chunk was freed
     */
    bool freePages(uint32_t index, uint32_t count);

    /**
     * \brief Adds a chunk to the allocator
     *
     * Adds the given region to the free list, so
     * that subsequent allocations can succeed.
     * \param [in] size Total chunk size, in bytes
     * \returns Chunk index
     */
    uint32_t addChunk(uint64_t size);

    /**
     * \brief Removes chunk from the allocator
     *
     * Must only be used if the entire chunk is unused.
     * \param [in] chunkIndex Chunk index
     */
    void removeChunk(uint32_t chunkIndex);

    /**
     * \brief Disables a chunk
     *
     * Makes an entire chunk unavailable for subsequent allocations.
     * This can be useful when moving allocations out of that chunk
     * in an attempt to free some memory.
     * \param [in] chunkIndex Chunk index
     */
    void killChunk(uint32_t chunkIndex);

    /**
     * \brief Re-enables a chunk
     *
     * Makes all disabled chunks available for allocations again.
     * Should be used before allocating new chunk memory.
     * \param [in] chunkIndex Chunk index
     */
    void reviveChunk(uint32_t chunkIndex);

    /**
     * \brief Re-enables all disabled chunks
     * \returns Number of chunks re-enabled
     */
    uint32_t reviveChunks();

    /**
     * \brief Queries page allocation mask
     *
     * Should be used for informational purposes only. Retrieves
     * a bit mask where each set bit represents an allocated page,
     * with the page index corresponding to the page index. The
     * output array must be sized appropriately.
     * \param [out] chunkIndex Chunk index
     * \param [out] pageMask Page mask
     */
    void getPageAllocationMask(uint32_t chunkIndex, uint32_t* pageMask) const;

  private:

    struct ChunkInfo {
      uint32_t  pageCount = 0u;
      uint32_t  pagesUsed = 0u;
      int32_t   nextChunk = -1;
      bool      disabled  = false;
    };

    struct PageRange {
      uint32_t  index = 0u;
      uint32_t  count = 0u;
    };

    std::vector<PageRange>  m_freeList;
    std::vector<int32_t>    m_freeListLutByPage;

    std::vector<ChunkInfo>  m_chunks;
    int32_t                 m_freeChunk = -1;

    int32_t searchFreeList(uint32_t count);

    void addLutEntry(const PageRange& range, int32_t index);

    void removeLutEntry(const PageRange& range);

    void insertFreeRange(PageRange newRange, int32_t currentIndex);

  };


  /**
   * \brief Pool allocator
   *
   * Implements an fast allocator for objects less than the size of one
   * page. Uses a regular page allocator to allocate backing storage for
   * each object pool.
   */
  class DxvkPoolAllocator {
    // Use the machine's native word size for bit masks to enable fast paths
    using MaskType = std::conditional_t<env::is32BitHostPlatform(), uint32_t, uint64_t>;

    constexpr static uint32_t MaskBits = sizeof(MaskType) * 8u;

    constexpr static uint32_t MaxCapacityBits = 8u;
    constexpr static uint32_t MaxCapacity = 1u << MaxCapacityBits;

    constexpr static uint32_t MasksPerPage = MaxCapacity / MaskBits;
  public:

    /// Allocation granularity. Smaller allocations are rounded up to
    /// be at least of this size.
    constexpr static uint64_t MinSize = DxvkPageAllocator::PageSize >> MaxCapacityBits;

    /// Minimum supported allocation size. Always set to half a page
    /// so that any pools we manage can at least hold two allocations.
    constexpr static uint64_t MaxSize = DxvkPageAllocator::PageSize >> 1u;

    DxvkPoolAllocator(DxvkPageAllocator& pageAllocator);

    ~DxvkPoolAllocator();

    /**
     * \brief Allocates given number of bytes from the pool
     *
     * \param [in] size Allocation size, in bytes
     * \returns Allocated address, in bytes
     */
    int64_t alloc(uint64_t size);

    /**
     * \brief Frees allocated memory region
     *
     * \param [in] address Memory address, in bytes
     * \param [in] size Allocation size, in bytes
     * \returns \c true if a chunk was freed
     */
    bool free(uint64_t address, uint64_t size);

  private:

    struct PageList {
      int32_t   head = -1;
      int32_t   tail = -1;
    };

    struct PageInfo {
      MaskType  pool = 0u;
      int32_t   prev = -1;
      int32_t   next = -1;
    };

    struct PagePool {
      int32_t   nextPool  = -1;
      uint16_t  freeMask  = 0u;
      uint16_t  usedMask  = 0u;

      std::array<MaskType, MasksPerPage> subPools = { };
    };

    DxvkPageAllocator*      m_pageAllocator = nullptr;

    std::vector<PageInfo>   m_pageInfos;
    std::vector<PagePool>   m_pagePools;
    int32_t                 m_freePool = -1;

    std::array<PageList, MaxCapacityBits> m_pageLists = { };

    int32_t allocPage(uint32_t listIndex);

    bool freePage(uint32_t pageIndex, uint32_t listIndex);

    void addPageToList(uint32_t pageIndex, uint32_t listIndex);

    void removePageFromList(uint32_t pageIndex, uint32_t listIndex);

    uint32_t allocPagePool(uint32_t capacity);

    void freePagePool(uint32_t poolIndex);

    static uint32_t computeListIndex(uint64_t size);

    static uint32_t computePoolCapacity(uint32_t index);

    static uint64_t computeByteAddress(uint32_t page, uint32_t index, uint32_t list);

    static uint32_t computePageIndexFromByteAddress(uint64_t address);

    static uint32_t computeItemIndexFromByteAddress(uint64_t address, uint32_t list);

  };

}
