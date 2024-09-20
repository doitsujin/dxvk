#pragma once

#include "dxvk_adapter.h"
#include "dxvk_allocator.h"

namespace dxvk {
  
  class DxvkMemoryAllocator;
  class DxvkMemoryChunk;
  
  /**
   * \brief Memory stats
   * 
   * Reports the amount of device memory
   * allocated and used by the application.
   */
  struct DxvkMemoryStats {
    VkDeviceSize memoryAllocated = 0;
    VkDeviceSize memoryUsed      = 0;
  };


  enum class DxvkSharedHandleMode {
      None,
      Import,
      Export,
  };

  /**
   * \brief Shared handle info
   *
   * The shared resource information for a given resource.
   */
  struct DxvkSharedHandleInfo {
    DxvkSharedHandleMode mode = DxvkSharedHandleMode::None;
    VkExternalMemoryHandleTypeFlagBits type   = VK_EXTERNAL_MEMORY_HANDLE_TYPE_FLAG_BITS_MAX_ENUM;
    union {
      // When we want to implement this on non-Windows platforms,
      // we could add a `int fd` here, etc.
      HANDLE handle = INVALID_HANDLE_VALUE;
    };
  };


  /**
   * \brief Device memory object
   * 
   * Stores a Vulkan memory object. If the object
   * was allocated on host-visible memory, it will
   * be persistently mapped.
   */
  struct DxvkDeviceMemory {
    VkBuffer              buffer  = VK_NULL_HANDLE;
    VkDeviceMemory        memory  = VK_NULL_HANDLE;
    VkDeviceSize          size    = 0;
    void*                 mapPtr  = nullptr;
  };

  
  /**
   * \brief Memory pool
   *
   * Stores a list of memory chunks, as well as an allocator
   * over the entire pool.
   */
  struct DxvkMemoryPool {
    constexpr static VkDeviceSize MaxChunkSize = DxvkPageAllocator::MaxChunkSize;
    constexpr static VkDeviceSize MinChunkSize = MaxChunkSize / 64u;

    /// Backing storage for allocated memory chunks
    std::vector<DxvkDeviceMemory> chunks;
    /// Memory allocator covering the entire memory pool
    DxvkPageAllocator pageAllocator;
    /// Pool allocator that sits on top of the page allocator
    DxvkPoolAllocator poolAllocator = { pageAllocator };
    /// Minimum desired allocation size for the next chunk.
    /// Always a power of two.
    VkDeviceSize nextChunkSize = MinChunkSize;
    /// Maximum chunk size for the memory pool. Hard limit.
    VkDeviceSize maxChunkSize = MaxChunkSize;

    force_inline int64_t alloc(uint64_t size, uint64_t align) {
      if (size <= DxvkPoolAllocator::MaxSize)
        return poolAllocator.alloc(size);
      else
        return pageAllocator.alloc(size, align);
    }

    force_inline bool free(uint64_t address, uint64_t size) {
      if (size <= DxvkPoolAllocator::MaxSize)
        return poolAllocator.free(address, size);
      else
        return pageAllocator.free(address, size);
    }
  };


  /**
   * \brief Memory heap
   * 
   * Corresponds to a Vulkan memory heap and stores
   * its properties as well as allocation statistics.
   */
  struct DxvkMemoryHeap {
    uint32_t          index         = 0u;
    uint32_t          memoryTypes   = 0u;
    VkMemoryHeap      properties    = { };
  };


  /**
   * \brief Memory type
   * 
   * Corresponds to a Vulkan memory type and stores
   * memory chunks used to sub-allocate memory on
   * this memory type.
   */
  struct DxvkMemoryType {
    uint32_t          index         = 0u;
    VkMemoryType      properties    = { };

    DxvkMemoryHeap*   heap          = nullptr;

    DxvkMemoryStats   stats         = { };

    VkBufferUsageFlags bufferUsage  = 0u;

    DxvkMemoryPool    devicePool;
    DxvkMemoryPool    mappedPool;
  };


  /**
   * \brief Memory type statistics
   */
  struct DxvkMemoryTypeStats {
    /// Memory type properties
    VkMemoryType properties = { };
    /// Amount of memory allocated
    VkDeviceSize allocated = 0u;
    /// Amount of memory used
    VkDeviceSize used = 0u;
    /// First chunk in the array
    size_t chunkIndex = 0u;
    /// Number of chunks allocated
    size_t chunkCount = 0u;
  };


  /**
   * \brief Chunk statistics
   */
  struct DxvkMemoryChunkStats {
    /// Chunk size, in bytes
    VkDeviceSize capacity = 0u;
    /// Used memory, in bytes
    VkDeviceSize used = 0u;
    /// Index of first page mask belonging to this
    /// chunk in the page mask array
    size_t pageMaskOffset = 0u;
    /// Number of pages in this chunk.
    size_t pageCount = 0u;
  };


  /**
   * \brief Detailed memory allocation statistics
   */
  struct DxvkMemoryAllocationStats {
    std::array<DxvkMemoryTypeStats, VK_MAX_MEMORY_TYPES> memoryTypes = { };
    std::vector<DxvkMemoryChunkStats> chunks;
    std::vector<uint32_t> pageMasks;
  };


  /**
   * \brief Memory slice
   * 
   * Represents a slice of memory that has
   * been sub-allocated from a bigger chunk.
   */
  class DxvkMemory {
    friend class DxvkMemoryAllocator;
  public:
    
    DxvkMemory();
    DxvkMemory(
      DxvkMemoryAllocator*  alloc,
      DxvkMemoryType*       type,
      VkBuffer              buffer,
      VkDeviceMemory        memory,
      VkDeviceSize          address,
      VkDeviceSize          length,
      void*                 mapPtr);
    DxvkMemory             (DxvkMemory&& other);
    DxvkMemory& operator = (DxvkMemory&& other);
    ~DxvkMemory();
    
    /**
     * \brief Memory object
     * 
     * This information is required when
     * binding memory to Vulkan objects.
     * \returns Memory object
     */
    VkDeviceMemory memory() const {
      return m_memory;
    }
    
    /**
     * \brief Buffer object
     *
     * Global buffer covering the entire memory allocation.
     * \returns Buffer object
     */
    VkBuffer buffer() const {
      return m_buffer;
    }

    /**
     * \brief Offset into device memory
     * 
     * This information is required when
     * binding memory to Vulkan objects.
     * \returns Offset into device memory
     */
    VkDeviceSize offset() const {
      return m_address & DxvkPageAllocator::ChunkAddressMask;
    }
    
    /**
     * \brief Pointer to mapped data
     * 
     * \param [in] offset Byte offset
     * \returns Pointer to mapped data
     */
    void* mapPtr(VkDeviceSize offset) const {
      return reinterpret_cast<char*>(m_mapPtr) + offset;
    }

    /**
     * \brief Returns length of memory allocated
     * 
     * \returns Memory size
     */
    VkDeviceSize length() const {
      return m_length;
    }

    /**
     * \brief Checks whether the memory slice is defined
     * 
     * \returns \c true if this slice points to actual device
     *          memory, and \c false if it is undefined.
     */
    explicit operator bool () const {
      return m_memory != VK_NULL_HANDLE;
    }

    /**
     * \brief Queries global buffer usage flags
     * \returns Global buffer usage flags, if any
     */
    VkBufferUsageFlags getBufferUsage() const {
      return m_buffer ? m_type->bufferUsage : 0u;
    }

  private:
    
    DxvkMemoryAllocator*  m_alloc  = nullptr;
    DxvkMemoryType*       m_type   = nullptr;
    VkBuffer              m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory        m_memory = VK_NULL_HANDLE;
    VkDeviceSize          m_address = 0;
    VkDeviceSize          m_length = 0;
    void*                 m_mapPtr = nullptr;
    
    void free();
    
  };


  /**
   * \brief Memory requirement info
   */
  struct DxvkMemoryRequirements {
    VkImageTiling                 tiling;
    VkMemoryDedicatedRequirements dedicated;
    VkMemoryRequirements2         core;
  };


  /**
   * \brief Memory allocation info
   */
  struct DxvkMemoryProperties {
    VkExportMemoryAllocateInfo    sharedExport;
    VkImportMemoryWin32HandleInfoKHR sharedImportWin32;
    VkMemoryDedicatedAllocateInfo dedicated;
    VkMemoryPropertyFlags         flags;
  };


  /**
   * \brief Memory allocator
   * 
   * Allocates device memory for Vulkan resources.
   * Memory objects will be destroyed automatically.
   */
  class DxvkMemoryAllocator {
    friend class DxvkMemory;
    friend class DxvkMemoryChunk;

    constexpr static uint64_t DedicatedChunkAddress = 1ull << 63u;

    constexpr static VkDeviceSize SmallAllocationThreshold = 256 << 10;

    constexpr static VkDeviceSize MinChunkSize =   4ull << 20;
    constexpr static VkDeviceSize MaxChunkSize = 256ull << 20;

    constexpr static VkDeviceSize MinResourcesPerChunk = 4u;
  public:
    
    DxvkMemoryAllocator(DxvkDevice* device);
    ~DxvkMemoryAllocator();
    
    DxvkDevice* device() const {
      return m_device;
    }

    /**
     * \brief Memory type mask for sparse resources
     * \returns Sparse resource memory types
     */
    uint32_t getSparseMemoryTypes() const {
      return m_sparseMemoryTypes;
    }

    /**
     * \brief Allocates device memory
     *
     * Legacy interface for memory allocation, to be removed.
     * \param [in] req Memory requirements
     * \param [in] info Memory properties
     * \returns Allocated memory slice
     */
    DxvkMemory alloc(
            DxvkMemoryRequirements            req,
      const DxvkMemoryProperties&             info);

    /**
     * \brief Allocates memory for a regular resource
     *
     * This method should be used when a dedicated allocation is
     * not required. Very large resources may still be placed in
     * a dedicated allocation.
     * \param [in] requirements Memory requirements
     * \param [in] properties Memory property flags. Some of
     *    these may be ignored in case of memory pressure.
     */
    DxvkMemory allocateMemory(
      const VkMemoryRequirements&             requirements,
            VkMemoryPropertyFlags             properties);

    /**
     * \brief Allocates memory for a resource
     *
     * Will always create a dedicated allocation.
     * \param [in] requirements Memory requirements
     * \param [in] properties Memory property flags. Some of
     *    these may be ignored in case of memory pressure.
     * \param [in] next Further memory properties
     */
    DxvkMemory allocateDedicatedMemory(
      const VkMemoryRequirements&             requirements,
            VkMemoryPropertyFlags             properties,
      const void*                             next);

    /**
     * \brief Queries memory stats
     * 
     * Returns the total amount of memory
     * allocated and used for a given heap.
     * \param [in] heap Heap index
     * \returns Memory stats for this heap
     */
    DxvkMemoryStats getMemoryStats(uint32_t heap) const;
    
    /**
     * \brief Retrieves detailed memory statistics
     *
     * Queries statistics for each memory type and each allocated chunk.
     * Can be useful to determine the degree of memory fragmentation.
     * \param [out] stats Memory statistics
     */
    void getAllocationStats(DxvkMemoryAllocationStats& stats);

    /**
     * \brief Queries buffer memory requirements
     *
     * Can be used to get memory requirements without having
     * to create a buffer object first.
     * \param [in] createInfo Buffer create info
     * \param [in,out] memoryRequirements Memory requirements
     */
    bool getBufferMemoryRequirements(
      const VkBufferCreateInfo&     createInfo,
            VkMemoryRequirements2&  memoryRequirements) const;

    /**
     * \brief Queries image memory requirements
     *
     * Can be used to get memory requirements without having
     * to create an image object first.
     * \param [in] createInfo Image create info
     * \param [in,out] memoryRequirements Memory requirements
     */
    bool getImageMemoryRequirements(
      const VkImageCreateInfo&      createInfo,
            VkMemoryRequirements2&  memoryRequirements) const;

  private:

    DxvkDevice* m_device;

    dxvk::mutex m_mutex;

    uint32_t m_memTypeCount = 0u;
    uint32_t m_memHeapCount = 0u;

    std::array<DxvkMemoryType, VK_MAX_MEMORY_TYPES> m_memTypes = { };
    std::array<DxvkMemoryHeap, VK_MAX_MEMORY_HEAPS> m_memHeaps = { };

    uint32_t m_sparseMemoryTypes = 0u;

    std::array<uint32_t, 16> m_memTypesByPropertyFlags = { };

    DxvkDeviceMemory allocateDeviceMemory(
            DxvkMemoryType&       type,
            VkDeviceSize          size,
      const void*                 next);

    bool allocateChunkInPool(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkMemoryPropertyFlags properties,
            VkDeviceSize          requiredSize,
            VkDeviceSize          desiredSize);

    DxvkMemory createMemory(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkDeviceSize          address,
            VkDeviceSize          size);

    void free(
      const DxvkMemory&           memory);

    void freeDeviceMemory(
            DxvkMemoryType&       type,
            DxvkDeviceMemory      memory);
    
    uint32_t countEmptyChunksInPool(
      const DxvkMemoryPool&       pool) const;

    void freeEmptyChunksInHeap(
      const DxvkMemoryHeap&       heap,
            VkDeviceSize          allocationSize);

    void freeEmptyChunksInPool(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkDeviceSize          allocationSize);

    int32_t findEmptyChunkInPool(
      const DxvkMemoryPool&       pool,
            VkDeviceSize          minSize,
            VkDeviceSize          maxSize) const;

    void mapDeviceMemory(
            DxvkDeviceMemory&     memory,
            VkMemoryPropertyFlags properties);

    DxvkMemory createMemory(
            DxvkMemoryType&       type,
      const DxvkMemoryPool&       pool,
            VkDeviceSize          address,
            VkDeviceSize          size);

    DxvkMemory createMemory(
            DxvkMemoryType&       type,
      const DxvkDeviceMemory&     memory);

    void getAllocationStatsForPool(
      const DxvkMemoryType&       type,
      const DxvkMemoryPool&       pool,
            DxvkMemoryAllocationStats& stats);

    VkDeviceSize determineMaxChunkSize(
      const DxvkMemoryType&       type,
            bool                  mappable) const;

    uint32_t determineSparseMemoryTypes(
            DxvkDevice*           device) const;

    void determineBufferUsageFlagsPerMemoryType();

    void determineMemoryTypesWithPropertyFlags();

    void logMemoryError(
      const VkMemoryRequirements& req) const;

    void logMemoryStats() const;

    bit::BitMask getMemoryTypeMask(
      const VkMemoryRequirements&             requirements,
            VkMemoryPropertyFlags             properties) const;

  };
  
}
