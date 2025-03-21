#pragma once

#include <map>
#include <memory>

#include "dxvk_access.h"
#include "dxvk_adapter.h"
#include "dxvk_allocator.h"
#include "dxvk_hash.h"

#include "../util/util_time.h"

namespace dxvk {
  
  class DxvkMemoryAllocator;
  class DxvkSparsePageTable;
  class DxvkSharedAllocationCache;
  class DxvkResourceAllocation;
  class DxvkPagedResource;
  struct DxvkMemoryChunk;

  /**
   * \brief Memory stats
   * 
   * Reports the amount of device memory
   * allocated and used by the application.
   */
  struct DxvkMemoryStats {
    VkDeviceSize memoryAllocated = 0;
    VkDeviceSize memoryUsed      = 0;
    VkDeviceSize memoryBudget    = 0;
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
    VkDeviceSize          size    = 0u;
    void*                 mapPtr  = nullptr;
    VkDeviceAddress       gpuVa   = 0u;
    uint64_t              cookie  = 0u;
  };


  /**
   * \brief Memory chunk
   *
   * Stores a device memory object with some metadata.
   */
  struct DxvkMemoryChunk {
    /// Backing storage for this chunk
    DxvkDeviceMemory memory;
    /// Time when the chunk has been marked as unused. Must
    /// be set to 0 when allocating memory from the chunk
    high_resolution_clock::time_point unusedTime = { };
    /// Unordered list of resources suballocated from this chunk.
    DxvkResourceAllocation* allocationList = nullptr;
    /// Whether defragmentation can be performed on this chunk.
    /// Only relevant for chunks in non-mappable device memory.
    VkBool32 canMove = true;

    void addAllocation(DxvkResourceAllocation* allocation);
    void removeAllocation(DxvkResourceAllocation* allocation);
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
    std::vector<DxvkMemoryChunk> chunks;
    /// Memory allocator covering the entire memory pool
    DxvkPageAllocator pageAllocator;
    /// Pool allocator that sits on top of the page allocator
    DxvkPoolAllocator poolAllocator = { pageAllocator };
    /// Minimum desired allocation size for the next chunk.
    /// Always a power of two.
    VkDeviceSize nextChunkSize = MinChunkSize;
    /// Maximum chunk size for the memory pool. Hard limit.
    VkDeviceSize maxChunkSize = MaxChunkSize;
    /// Next chunk to relocate for defragmentation
    uint32_t nextDefragChunk = ~0u;

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
    VkDeviceSize      memoryBudget  = 0u;
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

    DxvkSharedAllocationCache* sharedCache = nullptr;
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
    uint32_t pageMaskOffset = 0u;
    /// Number of pages in this chunk.
    uint16_t pageCount = 0u;
    /// Whether this chunk is mapped
    bool mapped = false;
    /// Whether this chunk is active
    bool active = false;
    /// Chunk cookie
    uint32_t cookie = 0u;
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
   * \brief Sharing mode info
   *
   * Stores available queue families and provides methods
   * to fill in sharing mode infos for resource creation.
   */
  struct DxvkSharingModeInfo {
    std::array<uint32_t, 2u> queueFamilies = { };

    VkSharingMode sharingMode() const {
      return queueFamilies[0] != queueFamilies[1]
        ? VK_SHARING_MODE_CONCURRENT
        : VK_SHARING_MODE_EXCLUSIVE;
    }

    template<typename CreateInfo>
    void fill(CreateInfo& info) const {
      info.sharingMode = sharingMode();

      if (info.sharingMode == VK_SHARING_MODE_CONCURRENT) {
        info.queueFamilyIndexCount = queueFamilies.size();
        info.pQueueFamilyIndices = queueFamilies.data();
      }
    }
  };


  /**
   * \brief Buffer view key
   *
   * Stores buffer view properties.
   */
  struct DxvkBufferViewKey {
    /// Buffer view format
    VkFormat format = VK_FORMAT_UNDEFINED;
    /// View usage. Must include one or both texel buffer flags.
    VkBufferUsageFlags usage = 0u;
    /// Buffer offset, in bytes
    VkDeviceSize offset = 0u;
    /// Buffer view size, in bytes
    VkDeviceSize size = 0u;

    size_t hash() const {
      DxvkHashState hash;
      hash.add(uint32_t(format));
      hash.add(uint32_t(usage));
      hash.add(offset);
      hash.add(size);
      return hash;
    }

    bool eq(const DxvkBufferViewKey& other) const {
      return format == other.format
          && usage  == other.usage
          && offset == other.offset
          && size   == other.size;
    }
  };


  /**
   * \brief Image view map
   */
  class DxvkResourceBufferViewMap {

  public:

    DxvkResourceBufferViewMap(
            DxvkMemoryAllocator*        allocator,
            VkBuffer                    buffer);

    ~DxvkResourceBufferViewMap();

    /**
     * \brief Creates a buffer view
     *
     * \param [in] key View properties
     * \param [in] baseOffset Buffer offset
     * \returns Buffer view handle
     */
    VkBufferView createBufferView(
      const DxvkBufferViewKey&          key,
            VkDeviceSize                baseOffset);

  private:

    Rc<vk::DeviceFn>  m_vkd;
    VkBuffer          m_buffer          = VK_NULL_HANDLE;
    bool              m_passBufferUsage = false;

    dxvk::mutex       m_mutex;
    std::unordered_map<DxvkBufferViewKey,
      VkBufferView, DxvkHash, DxvkEq> m_views;

  };


  /**
   * \brief Image view key
   *
   * Stores a somewhat compressed representation
   * of image view properties.
   */
  struct DxvkImageViewKey {
    /// View type
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    /// View usage flags
    VkImageUsageFlags usage = 0u;
    /// View format
    VkFormat format = VK_FORMAT_UNDEFINED;
    /// Aspect flags to include in this view
    VkImageAspectFlags aspects = 0u;
    /// First mip
    uint8_t mipIndex = 0u;
    /// Number of mips
    uint8_t mipCount = 0u;
    /// First array layer
    uint16_t layerIndex = 0u;
    /// Number of array layers
    uint16_t layerCount = 0u;
    /// Packed component swizzle, with four bits per component
    uint16_t packedSwizzle = 0u;

    size_t hash() const {
      DxvkHashState hash;
      hash.add(uint32_t(viewType));
      hash.add(uint32_t(usage));
      hash.add(uint32_t(format));
      hash.add(uint32_t(aspects));
      hash.add(uint32_t(mipIndex) | (uint32_t(mipCount) << 16));
      hash.add(uint32_t(layerIndex) | (uint32_t(layerCount) << 16));
      hash.add(uint32_t(packedSwizzle));
      return hash;
    }

    bool eq(const DxvkImageViewKey& other) const {
      return viewType == other.viewType
          && usage == other.usage
          && format == other.format
          && aspects == other.aspects
          && mipIndex == other.mipIndex
          && mipCount == other.mipCount
          && layerIndex == other.layerIndex
          && layerCount == other.layerCount
          && packedSwizzle == other.packedSwizzle;
    }

    VkComponentMapping unpackSwizzle() const {
      return VkComponentMapping {
        VkComponentSwizzle((packedSwizzle >>  0) & 0xf),
        VkComponentSwizzle((packedSwizzle >>  4) & 0xf),
        VkComponentSwizzle((packedSwizzle >>  8) & 0xf),
        VkComponentSwizzle((packedSwizzle >> 12) & 0xf) };
    }

    static uint16_t packSwizzle(VkComponentMapping mapping) {
      return (uint16_t(mapping.r) <<  0)
           | (uint16_t(mapping.g) <<  4)
           | (uint16_t(mapping.b) <<  8)
           | (uint16_t(mapping.a) << 12);
    }
  };


  /**
   * \brief Image view map
   */
  class DxvkResourceImageViewMap {

  public:

    DxvkResourceImageViewMap(
            DxvkMemoryAllocator*        allocator,
            VkImage                     image);

    ~DxvkResourceImageViewMap();

    /**
     * \brief Creates an image view
     *
     * \param [in] key View properties
     * \returns Image view handle
     */
    VkImageView createImageView(
      const DxvkImageViewKey&           key);

  private:

    Rc<vk::DeviceFn>  m_vkd;
    VkImage           m_image = VK_NULL_HANDLE;

    dxvk::mutex       m_mutex;
    std::unordered_map<DxvkImageViewKey,
      VkImageView, DxvkHash, DxvkEq> m_views;

  };


  /**
   * \brief Memory properties
   */
  struct DxvkResourceMemoryInfo {
    /// Vulkan memory handle
    VkDeviceMemory memory = VK_NULL_HANDLE;
    /// Offset into memory object
    VkDeviceSize offset = 0u;
    /// Size of memory range
    VkDeviceSize size = 0u;
  };


  /**
   * \brief Buffer properties
   */
  struct DxvkResourceBufferInfo {
    /// Buffer handle
    VkBuffer buffer = VK_NULL_HANDLE;
    /// Buffer offset, in bytes
    VkDeviceSize offset = 0u;
    /// Buffer size, in bytes
    VkDeviceSize size = 0u;
    /// Pointer to mapped memory region
    void* mapPtr = nullptr;
    /// GPU address of the buffer
    VkDeviceSize gpuAddress = 0u;
  };


  /**
   * \brief Image properties
   */
  struct DxvkResourceImageInfo {
    /// Image handle
    VkImage image = VK_NULL_HANDLE;
    /// Pointer to mapped memory region
    void* mapPtr = nullptr;
  };


  /**
   * \brief Resource allocation flags
   */
  enum class DxvkAllocationFlag : uint32_t {
    /// Allocation owns the given VkDeviceMemory allocation
    /// and is not suballocated from an existing chunk.
    OwnsMemory  = 0,
    /// Allocation owns a dedicated VkBuffer object rather
    /// than the global buffer for the parent chunk, if any.
    OwnsBuffer  = 1,
    /// Allocation owns a VkImage object.
    OwnsImage   = 2,
    /// Allocation can use an allocation cache.
    CanCache    = 3,
    /// Allocation can be relocated for defragmentation.
    CanMove     = 4,
    /// Allocation is imported from an external API.
    Imported    = 5,
    /// Memory must be cleared to zero when the allocation
    /// is freed. Only used to work around app bugs.
    ClearOnFree = 6,
  };

  using DxvkAllocationFlags = Flags<DxvkAllocationFlag>;


  /**
   * \brief Vulkan resource with memory allocation
   *
   * Reference-counted object that stores a Vulkan resource together
   * with the memory allocation backing the resource, as well as views
   * created from that resource.
   */
  class alignas(CACHE_LINE_SIZE) DxvkResourceAllocation {
    friend DxvkMemoryAllocator;

    friend struct DxvkMemoryChunk;
    friend class DxvkLocalAllocationCache;
    friend class DxvkSharedAllocationCache;
  public:

    DxvkResourceAllocation(
            DxvkMemoryAllocator*        allocator,
            DxvkMemoryType*             type)
    : m_allocator(allocator), m_type(type) { }

    ~DxvkResourceAllocation();

    /**
     * \brief Increments reference count
     */
    force_inline void incRef() {
      m_useCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements reference count
     * Frees allocation if necessary
     */
    force_inline void decRef() {
      if (unlikely(m_useCount.fetch_sub(1u, std::memory_order_acquire) == 1u))
        free();
    }

    /**
     * \brief Queries allocation flags
     * \returns Allocation flags
     */
    DxvkAllocationFlags flags() const {
      return m_flags;
    }

    /**
     * \brief Queries mapped memory region
     * \returns Mapped memory region
     */
    void* mapPtr() const {
      return m_mapPtr;
    }

    /**
     * \brief Queries memory info
     * \returns Memory info
     */
    DxvkResourceMemoryInfo getMemoryInfo() const {
      DxvkResourceMemoryInfo result = { };
      result.memory = m_memory;
      result.offset = m_address & DxvkPageAllocator::ChunkAddressMask;
      result.size = m_size;
      return result;
    }

    /**
     * \brief Queries buffer info
     * \returns Buffer info
     */
    DxvkResourceBufferInfo getBufferInfo() const {
      DxvkResourceBufferInfo result = { };
      result.buffer = m_buffer;
      result.offset = m_bufferOffset;
      result.size = m_size;
      result.mapPtr = m_mapPtr;
      result.gpuAddress = m_bufferAddress;
      return result;
    }

    /**
     * \brief Queries image info
     * \returns Image info
     */
    DxvkResourceImageInfo getImageInfo() const {
      DxvkResourceImageInfo result = { };
      result.image = m_image;
      result.mapPtr = m_mapPtr;
      return result;
    }

    /**
     * \brief Queries sparse page table
     *
     * Only applies to sparse resources.
     * \returns Pointer to sparse page table
     */
    DxvkSparsePageTable* getSparsePageTable() const {
      return m_sparsePageTable;
    }

    /**
     * \brief Queries memory property flags
     *
     * May be 0 for imported or foreign resources.
     * \returns Memory property flags
     */
    VkMemoryPropertyFlags getMemoryProperties() const {
      return m_type ? m_type->properties.propertyFlags : 0u;
    }

    /**
     * \brief Creates buffer view
     *
     * \param [in] key View properties
     * \returns Buffer view handle
     */
    VkBufferView createBufferView(
      const DxvkBufferViewKey&          key);

    /**
     * \brief Creates image view
     *
     * \param [in] key View properties
     * \returns Image view handle
     */
    VkImageView createImageView(
      const DxvkImageViewKey&           key);

  private:

    std::atomic<uint32_t>       m_useCount = { 0u };
    DxvkAllocationFlags         m_flags = 0u;

    uint64_t                    m_resourceCookie = 0u;

    VkDeviceMemory              m_memory = VK_NULL_HANDLE;
    VkDeviceSize                m_address = 0u;
    VkDeviceSize                m_size = 0u;
    void*                       m_mapPtr = nullptr;

    VkBuffer                    m_buffer = VK_NULL_HANDLE;
    VkDeviceSize                m_bufferOffset = 0u;
    VkDeviceAddress             m_bufferAddress = 0u;
    DxvkResourceBufferViewMap*  m_bufferViews = nullptr;

    VkImage                     m_image = VK_NULL_HANDLE;
    DxvkResourceImageViewMap*   m_imageViews = nullptr;

    DxvkSparsePageTable*        m_sparsePageTable = nullptr;

    DxvkMemoryAllocator*        m_allocator = nullptr;
    DxvkMemoryType*             m_type = nullptr;

    DxvkResourceAllocation*     m_nextCached = nullptr;

    DxvkResourceAllocation*     m_prevInChunk = nullptr;
    DxvkResourceAllocation*     m_nextInChunk = nullptr;

    void destroyBufferViews();

    void free();

    static force_inline uint64_t getIncrement(DxvkAccess access) {
      return uint64_t(1u) << (20u * uint32_t(access));
    }

  };


  /**
   * \brief Resource allocation pool
   *
   * Creates and recycles resource allocation objects.
   */
  class DxvkResourceAllocationPool {

  public:

    DxvkResourceAllocationPool();

    ~DxvkResourceAllocationPool();

    template<typename... Args>
    DxvkResourceAllocation* create(Args&&... args) {
      return new (alloc()) DxvkResourceAllocation(std::forward<Args>(args)...);
    }

    void free(DxvkResourceAllocation* allocation) {
      allocation->~DxvkResourceAllocation();
      recycle(allocation);
    }

  private:

    struct Storage {
      alignas(DxvkResourceAllocation)
      char data[sizeof(DxvkResourceAllocation)];
    };

    struct StorageList {
      StorageList(StorageList* next_)
      : next(next_) { }

      StorageList* next = nullptr;
    };

    struct StoragePool {
      std::array<Storage, 1023> objects;
      std::unique_ptr<StoragePool> next;
    };

    std::unique_ptr<StoragePool>  m_pool;
    StorageList*                  m_next = nullptr;

    void* alloc() {
      if (unlikely(!m_next))
        createPool();

      StorageList* list = m_next;
      m_next = list->next;
      list->~StorageList();

      auto storage = std::launder(reinterpret_cast<Storage*>(list));
      return storage->data;
    }

    void recycle(void* allocation) {
      auto storage = std::launder(reinterpret_cast<Storage*>(allocation));
      m_next = new (storage->data) StorageList(m_next);
    }

    void createPool();

  };


  /**
   * \brief Local allocation cache
   *
   * Provides pre-allocated memory of supported power-of-two sizes
   * in a non-thread safe manner. This is intended to be used for
   * context classes in order to reduce lock contention.
   */
  class DxvkLocalAllocationCache {
    friend DxvkMemoryAllocator;
  public:
    // Cache allocations up to 128 kiB
    constexpr static uint32_t PoolCount = 10u;

    constexpr static VkDeviceSize MinSize = DxvkPoolAllocator::MinSize;
    constexpr static VkDeviceSize MaxSize = MinSize << (PoolCount - 1u);

    constexpr static VkDeviceSize PoolCapacityInBytes = 4u * DxvkPageAllocator::PageSize;

    DxvkLocalAllocationCache() = default;

    DxvkLocalAllocationCache(
            DxvkMemoryAllocator*        allocator,
            uint32_t                    memoryTypes)
    : m_allocator(allocator), m_memoryTypes(memoryTypes) { }

    DxvkLocalAllocationCache(DxvkLocalAllocationCache&& other)
    : m_allocator(other.m_allocator), m_memoryTypes(other.m_memoryTypes),
      m_pools(other.m_pools) {
      other.m_allocator = nullptr;
      other.m_memoryTypes = 0u;
      other.m_pools = { };
    }

    DxvkLocalAllocationCache& operator = (DxvkLocalAllocationCache&& other) {
      freeCache();

      m_allocator = other.m_allocator;
      m_memoryTypes = other.m_memoryTypes;
      m_pools = other.m_pools;

      other.m_allocator = nullptr;
      other.m_memoryTypes = 0u;
      other.m_pools = { };
      return *this;
    }

    ~DxvkLocalAllocationCache() {
      freeCache();
    }

    /**
     * \brief Computes preferred number of cached allocations
     *
     * Depends on size so that a large enough number of consecutive
     * allocations can be handled by the local cache without wasting
     * too much memory on larger allocations.
     * \param [in] size Allocation size
     */
    static uint32_t computePreferredAllocationCount(
            VkDeviceSize                size);

    /**
     * \brief Computes pool index for a given allocation size
     *
     * \param [in] size Allocation size
     * \returns Pool index
     */
    static uint32_t computePoolIndex(
            VkDeviceSize                size);

    /**
     * \brief Computes allocation size for a given index
     *
     * \param [in] poolIndex Pool index
     * \returns Allocation size for the pool
     */
    static VkDeviceSize computeAllocationSize(
            uint32_t                    index);

  private:

    DxvkMemoryAllocator*  m_allocator   = nullptr;
    uint32_t              m_memoryTypes = 0u;

    std::array<DxvkResourceAllocation*, PoolCount> m_pools = { };

    DxvkResourceAllocation* allocateFromCache(
            VkDeviceSize                size);

    DxvkResourceAllocation* assignCache(
            VkDeviceSize                size,
            DxvkResourceAllocation*     allocation);

    void freeCache();

  };


  /**
   * \brief Allocation cache stats
   *
   * Keeps track of the number of requests as
   * well as the total size of the cache.
   */
  struct DxvkSharedAllocationCacheStats {
    /// Total number of requests
    uint32_t requestCount = 0u;
    /// Number of failed requests
    uint32_t missCount = 0u;
    /// Cache size, in bytes
    VkDeviceSize size = 0u;
  };


  /**
   * \brief Shared allocation cache
   *
   * Accumulates small allocations in free lists
   * that can be allocated in their entirety.
   */
  class DxvkSharedAllocationCache {
    constexpr static uint32_t PoolCount = DxvkLocalAllocationCache::PoolCount;
    constexpr static uint32_t PoolSize = PoolCount * (env::is32BitHostPlatform() ? 6u : 12u);

    constexpr static VkDeviceSize PoolCapacityInBytes = DxvkLocalAllocationCache::PoolCapacityInBytes;

    friend DxvkMemoryAllocator;
  public:

    DxvkSharedAllocationCache(
            DxvkMemoryAllocator*        allocator);

    ~DxvkSharedAllocationCache();

    /**
     * \brief Retrieves list of cached allocations
     *
     * \param [in] allocationSize Required allocation size
     * \returns Pointer to head of allocation list,
     *    or \c nullptr if the cache is empty.
     */
    DxvkResourceAllocation* getAllocationList(
            VkDeviceSize                allocationSize);

    /**
     * \brief Frees cacheable allocation
     *
     * \param [in] allocation Allocation to free
     * \returns List to destroy if the cache is full. Usually,
     *    \c nullptr if the allocation was successfully added.
     */
    DxvkResourceAllocation* freeAllocation(
            DxvkResourceAllocation*     allocation);

    /**
     * \brief Queries statistics
     * \returns Cache statistics
     */
    DxvkSharedAllocationCacheStats getStats();

    /**
     * \brief Frees unused memory
     *
     * Periodically called from the worker to free some
     * memory that has not been used in some time.
     * \param [in] time Current time
     */
    void cleanupUnusedFromLockedAllocator(
            high_resolution_clock::time_point time);

  private:

    struct FreeList {
      uint16_t size = 0u;
      uint16_t capacity = 0u;

      DxvkResourceAllocation* head = nullptr;
    };

    struct List {
      DxvkResourceAllocation* head = nullptr;
      int32_t                 next = -1;
    };

    struct Pool {
      int32_t   listIndex = -1;
      uint32_t  listCount = 0u;
      high_resolution_clock::time_point drainTime = { };
    };

    alignas(CACHE_LINE_SIZE)
    DxvkMemoryAllocator*        m_allocator = nullptr;

    dxvk::mutex                 m_freeMutex;
    std::array<FreeList, PoolCount> m_freeLists = { };

    alignas(CACHE_LINE_SIZE)
    dxvk::mutex                 m_poolMutex;
    std::array<Pool, PoolCount> m_pools = { };
    std::array<List, PoolSize>  m_lists = { };
    int32_t                     m_nextList = -1;

    uint32_t                    m_numRequests = 0u;
    uint32_t                    m_numMisses = 0u;

    VkDeviceSize                m_cacheSize = 0u;
    VkDeviceSize                m_maxCacheSize = 0u;

  };


  /**
   * \brief Buffer import info
   *
   * Used to import an existing Vulkan buffer. Note
   * that imported buffers must not be renamed.
   */
  struct DxvkBufferImportInfo {
    /// Buffer handle
    VkBuffer buffer = VK_NULL_HANDLE;
    /// Buffer offset
    VkDeviceSize offset = 0;
    /// Pointer to mapped memory region
    void* mapPtr = nullptr;
  };


  /**
   * \brief Allocation modes
   */
  enum class DxvkAllocationMode : uint32_t {
    /// If set, the allocation will fail if video memory is
    /// full rather than falling back to system memory.
    NoFallback      = 0,
    /// If set, the allocation will only succeed if it
    /// can be suballocated from an existing chunk.
    NoAllocation    = 1,
    /// Avoid using a dedicated allocation for this resource
    NoDedicated     = 2,

    eFlagEnum
  };

  using DxvkAllocationModes = Flags<DxvkAllocationMode>;


  /**
   * \brief Allocation properties
   */
  struct DxvkAllocationInfo {
    /// Virtual resource cookie for the allocation
    uint64_t resourceCookie = 0u;
    /// Desired memory property flags
    VkMemoryPropertyFlags properties = 0u;
    /// Allocation mode flags
    DxvkAllocationModes mode = 0u;
  };


  /**
   * \brief Relocation entry
   */
  struct DxvkRelocationEntry {
    DxvkRelocationEntry() = default;
    DxvkRelocationEntry(Rc<DxvkPagedResource>&& r, DxvkAllocationModes m)
    : resource(std::move(r)), mode(m) { }

    /// Resource to relocate
    Rc<DxvkPagedResource> resource;
    /// Resource to relocate
    DxvkAllocationModes mode = 0u;
  };


  /**
   * \brief Resource relocation helper
   *
   * Simple thread-safe data structure used to pass a list of
   * resources to move from the allocator to the CS thread.
   */
  class DxvkRelocationList {

  public:

    DxvkRelocationList();

    ~DxvkRelocationList();

    /**
     * \brief Retrieves list of resources to move
     *
     * Removes items from the internally stored list.
     * Any duplicate entries will be removed.
     * \param [in] count Number of entries to return
     * \param [in] size Maximum total resource size
     * \returns List of resources to move
     */
    std::vector<DxvkRelocationEntry> poll(
            uint32_t                    count,
            VkDeviceSize                size);

    /**
     * \brief Adds relocation entry to the list
     *
     * \param [in] resource Resource to add
     * \param [in] allocation Resource storage
     * \param [in] mode Allocation mode
     */
    void addResource(
            Rc<DxvkPagedResource>&&     resource,
      const DxvkResourceAllocation*     allocation,
            DxvkAllocationModes         mode);

    /**
     * \brief Clears list
     */
    void clear();

    /**
     * \brief Checks whether resource list is empty
     * \returns \c true if the list is empty
     */
    bool empty() {
      return m_entries.empty();
    }

  private:

    struct RelocationOrdering {
      bool operator () (const DxvkResourceMemoryInfo& a, const DxvkResourceMemoryInfo& b) const {
        // Keep chunks together, then order by offset in order to increase
        // the likelihood of freeing up a contiguous block of memory.
        if (a.memory < b.memory) return true;
        if (a.memory > b.memory) return false;
        return a.offset > b.offset;
      }
    };

    dxvk::mutex                 m_mutex;

    std::map<
      DxvkResourceMemoryInfo,
      DxvkRelocationEntry,
      RelocationOrdering>       m_entries;

  };


  /**
   * \brief Memory allocator
   * 
   * Allocates device memory for Vulkan resources.
   * Memory objects will be destroyed automatically.
   */
  class DxvkMemoryAllocator {
    friend DxvkResourceAllocation;
    friend DxvkLocalAllocationCache;
    friend DxvkSharedAllocationCache;

    constexpr static uint64_t DedicatedChunkAddress = 1ull << 63u;

    constexpr static VkDeviceSize MinChunkSize =   4ull << 20;
    constexpr static VkDeviceSize MaxChunkSize = 256ull << 20;

    // Assume an alignment of 256 bytes. This is enough to satisfy all
    // buffer use cases, and matches our minimum allocation size.
    constexpr static VkDeviceSize GlobalBufferAlignment = 256u;

    // Minimum number of allocations we want to be able to fit into a heap
    constexpr static uint32_t MinAllocationsPerHeap = 7u;
  public:
    
    DxvkMemoryAllocator(DxvkDevice* device);
    ~DxvkMemoryAllocator();
    
    DxvkDevice* device() const {
      return m_device;
    }

    /**
     * \brief Allocates memory for a regular resource
     *
     * This method should be used when a dedicated allocation is
     * not required. Very large resources may still be placed in
     * a dedicated allocation.
     * \param [in] requirements Memory requirements
     * \param [in] allocationInfo Allocation info
     * \param [in] properties Memory property flags. Some of
     *    these may be ignored in case of memory pressure.
     * \returns Allocated memory
     */
    Rc<DxvkResourceAllocation> allocateMemory(
      const VkMemoryRequirements&             requirements,
      const DxvkAllocationInfo&               allocationInfo);

    /**
     * \brief Allocates memory for a resource
     *
     * Will always create a dedicated allocation.
     * \param [in] requirements Memory requirements
     * \param [in] allocationInfo Allocation info
     * \param [in] next Further memory properties
     * \returns Allocated memory
     */
    Rc<DxvkResourceAllocation> allocateDedicatedMemory(
      const VkMemoryRequirements&             requirements,
      const DxvkAllocationInfo&               allocationInfo,
      const void*                             next);

    /**
     * \brief Creates buffer resource
     *
     * Will make use of global buffers whenever possible, but
     * may fall back to creating a dedicated Vulkan buffer.
     * \param [in] createInfo Buffer create info
     * \param [in] allocationInfo Allocation properties
     * \param [in] allocationCache Optional allocation cache
     * \returns Buffer resource
     */
    Rc<DxvkResourceAllocation> createBufferResource(
      const VkBufferCreateInfo&         createInfo,
      const DxvkAllocationInfo&         allocationInfo,
            DxvkLocalAllocationCache*   allocationCache);

    /**
     * \brief Creates image resource
     *
     * \param [in] createInfo Image create info
     * \param [in] allocationInfo Allocation properties
     * \param [in] next External memory properties
     * \returns Image resource
     */
    Rc<DxvkResourceAllocation> createImageResource(
      const VkImageCreateInfo&          createInfo,
      const DxvkAllocationInfo&         allocationInfo,
      const void*                       next);

    /**
     * \brief Creates allocation for sparse binding
     *
     * Allocates a single page of memory for sparse binding.
     * \returns Allocated memory region
     */
    Rc<DxvkResourceAllocation> createSparsePage();

    /**
     * \brief Creates local allocation cache for buffer resources
     *
     * \param [in] bufferUsage Required buffer usage flags
     * \param [in] properties Required memory properties
     * \returns Local allocation cache
     */
    DxvkLocalAllocationCache createAllocationCache(
            VkBufferUsageFlags          bufferUsage,
            VkMemoryPropertyFlags       properties);

    /**
     * \brief Imports existing buffer resource
     *
     * \param [in] createInfo Buffer create info
     * \param [in] importInfo Buffer import info
     * \returns Buffer resource
     */
    Rc<DxvkResourceAllocation> importBufferResource(
      const VkBufferCreateInfo&         createInfo,
      const DxvkAllocationInfo&         allocationInfo,
      const DxvkBufferImportInfo&       importInfo);

    /**
     * \brief Imports existing image resource
     *
     * \param [in] createInfo Image create info
     * \param [in] imageHandle Image handle
     * \returns Image resource
     */
    Rc<DxvkResourceAllocation> importImageResource(
      const VkImageCreateInfo&          createInfo,
      const DxvkAllocationInfo&         allocationInfo,
            VkImage                     imageHandle);

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
     * \brief Queries shared cache stats
     *
     * Returns statistics for all shared caches.
     * \returns Shared cache stats
     */
    DxvkSharedAllocationCacheStats getAllocationCacheStats() const;

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

    /**
     * \brief Registers a paged resource with cookie
     *
     * Useful when the allocator needs to track resources.
     * \param [in] resource Resource to add
     */
    void registerResource(
            DxvkPagedResource*          resource);

    /**
     * \brief Unregisters a paged resource
     * \param [in] resource Resource to remove
     */
    void unregisterResource(
            DxvkPagedResource*          resource);

    /**
     * \brief Locks an allocation in place
     *
     * Ensures that the resource is marked as immovable so
     * that defragmentation won't attempt to relocate it.
     */
    void lockResourceGpuAddress(
      const Rc<DxvkResourceAllocation>& allocation);

    /**
     * \brief Performs clean-up tasks
     *
     * Intended to be called periodically by a worker thread in order
     * to initiate defragmentation, clean up the allocation cache and
     * free unused memory.
     */
    void performTimedTasks();

    /**
     * \brief Polls relocation list
     *
     * \param [in] count Maximum resource count
     * \param [in] size Maximum total size
     * \returns Relocation entries
     */
    auto pollRelocationList(uint32_t count, VkDeviceSize size) {
      return m_relocations.poll(count, size);
    }

  private:

    DxvkDevice* m_device;

    DxvkSharingModeInfo       m_sharingModeInfo;

    dxvk::mutex               m_mutex;

    uint32_t m_memTypeCount = 0u;
    uint32_t m_memHeapCount = 0u;

    std::array<DxvkMemoryType, VK_MAX_MEMORY_TYPES> m_memTypes = { };
    std::array<DxvkMemoryHeap, VK_MAX_MEMORY_HEAPS> m_memHeaps = { };

    VkBufferUsageFlags  m_globalBufferUsageFlags = 0u;
    uint32_t            m_globalBufferMemoryTypes = 0u;

    uint32_t            m_sparseMemoryTypes = 0u;

    std::array<uint32_t, 16> m_memTypesByPropertyFlags = { };

    DxvkResourceAllocationPool  m_allocationPool;

    uint64_t            m_nextCookie = 0u;

    alignas(CACHE_LINE_SIZE)
    high_resolution_clock::time_point m_taskDeadline = { };
    std::array<DxvkMemoryStats, VK_MAX_MEMORY_HEAPS> m_adapterHeapStats = { };

    alignas(CACHE_LINE_SIZE)
    dxvk::mutex               m_resourceMutex;
    std::unordered_map<uint64_t, DxvkPagedResource*> m_resourceMap;

    alignas(CACHE_LINE_SIZE)
    DxvkRelocationList        m_relocations;

    DxvkDeviceMemory allocateDeviceMemory(
            DxvkMemoryType&       type,
            VkDeviceSize          size,
      const void*                 next);

    void assignMemoryDebugName(
      const DxvkDeviceMemory&     memory,
      const DxvkMemoryType&       type);

    bool allocateChunkInPool(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkMemoryPropertyFlags properties,
            VkDeviceSize          requiredSize,
            VkDeviceSize          desiredSize);

    void freeDeviceMemory(
            DxvkMemoryType&       type,
            DxvkDeviceMemory      memory);

    void freeAllocation(
            DxvkResourceAllocation* allocation);

    void freeLocalCache(
            DxvkLocalAllocationCache* cache);

    void freeCachedAllocations(
            DxvkResourceAllocation* allocation);

    void freeCachedAllocationsLocked(
            DxvkResourceAllocation* allocation);

    uint32_t countEmptyChunksInPool(
      const DxvkMemoryPool&       pool) const;

    void freeEmptyChunksInHeap(
      const DxvkMemoryHeap&       heap,
            VkDeviceSize          allocationSize,
            high_resolution_clock::time_point time);

    bool freeEmptyChunksInPool(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkDeviceSize          allocationSize,
            high_resolution_clock::time_point time);

    int32_t findEmptyChunkInPool(
      const DxvkMemoryPool&       pool,
            VkDeviceSize          minSize,
            VkDeviceSize          maxSize) const;

    void mapDeviceMemory(
            DxvkDeviceMemory&     memory,
            VkMemoryPropertyFlags properties);

    DxvkResourceAllocation* createAllocation(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkDeviceSize          address,
            VkDeviceSize          size,
      const DxvkAllocationInfo&   allocationInfo);

    DxvkResourceAllocation* createAllocation(
            DxvkMemoryType&       type,
      const DxvkDeviceMemory&     memory,
      const DxvkAllocationInfo&   allocationInfo);

    DxvkResourceAllocation* createAllocation(
            DxvkSparsePageTable*  sparsePageTable,
      const DxvkAllocationInfo&   allocationInfo);

    bool refillAllocationCache(
            DxvkLocalAllocationCache* cache,
      const VkMemoryRequirements& requirements,
            VkMemoryPropertyFlags properties);

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

    VkDeviceAddress getBufferDeviceAddress(
            VkBuffer              buffer) const;

    void logMemoryError(
      const VkMemoryRequirements& req) const;

    void logMemoryStats() const;

    uint32_t getMemoryTypeMask(
            VkMemoryPropertyFlags properties) const;

    uint32_t findGlobalBufferMemoryTypeMask(
            VkBufferUsageFlags    usage) const;

    void updateMemoryHeapBudgets();

    void updateMemoryHeapStats(
            uint32_t              heapIndex);

    void moveDefragChunk(
            DxvkMemoryType&       type);

    void pickDefragChunk(
            DxvkMemoryType&       type);

    void performTimedTasksLocked(
            high_resolution_clock::time_point currentTime);

  };
  


  inline void DxvkResourceAllocation::free() {
    m_allocator->freeAllocation(this);
  }

}
