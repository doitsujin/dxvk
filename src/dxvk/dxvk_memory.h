#pragma once

#include <memory>

#include "dxvk_adapter.h"
#include "dxvk_allocator.h"
#include "dxvk_hash.h"

#include "../util/util_time.h"

namespace dxvk {
  
  class DxvkMemoryAllocator;
  class DxvkMemoryChunk;
  class DxvkSparsePageTable;

  /**
   * \brief Resource access flags
   */
  enum class DxvkAccess : uint32_t {
    None    = 0,
    Read    = 1,
    Write   = 2,
  };

  using DxvkAccessFlags = Flags<DxvkAccess>;


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
    uint32_t pageMaskOffset = 0u;
    /// Number of pages in this chunk.
    uint16_t pageCount = 0u;
    /// Whether this chunk is mapped
    bool mapped = false;
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
    OwnsMemory  = 0,
    OwnsBuffer  = 1,
    OwnsImage   = 2,
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
    friend class DxvkMemory;
  public:

    DxvkResourceAllocation(
            DxvkMemoryAllocator*        allocator,
            DxvkMemoryType*             type)
    : m_allocator(allocator), m_type(type) { }

    ~DxvkResourceAllocation();

    force_inline void incRef() { acquire(DxvkAccess::None); }
    force_inline void decRef() { release(DxvkAccess::None); }

    /**
     * \brief Releases allocation
     *
     * Increments the use counter of the allocation.
     * \param [in] access Resource access
     */
    force_inline void acquire(DxvkAccess access) {
      m_useCount.fetch_add(getIncrement(access), std::memory_order_acquire);
    }

    /**
     * \brief Releases allocation
     *
     * Decrements the use counter and frees the allocation if necessary.
     * \param [in] access Resource access
     */
    force_inline void release(DxvkAccess access) {
      uint64_t increment = getIncrement(access);
      uint64_t remaining = m_useCount.fetch_sub(increment, std::memory_order_release) - increment;

      if (unlikely(!remaining))
        free();
    }

    /**
     * \brief Checks whether the resource is in use
     *
     * Note that when checking for read access, this will also
     * return \c true if the resource is being written to.
     * \param [in] access Access to check
     */
    force_inline bool isInUse(DxvkAccess access) const {
      uint64_t cur = m_useCount.load(std::memory_order_acquire);
      return cur >= getIncrement(access);
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

    std::atomic<uint64_t>       m_useCount = { 0u };

    uint32_t                    m_resourceCookie = 0u;
    DxvkAllocationFlags         m_flags = 0u;

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

    void free();

    static force_inline uint64_t getIncrement(DxvkAccess access) {
      return uint64_t(1u) << (20u * uint32_t(access));
    }

  };

  static_assert(sizeof(DxvkResourceAllocation) == 2u * CACHE_LINE_SIZE);


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
    Rc<DxvkResourceAllocation> create(Args&&... args) {
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
   * \brief Memory slice
   * 
   * Represents a slice of memory that has
   * been sub-allocated from a bigger chunk.
   */
  struct DxvkMemory {
    DxvkMemory() = default;

    explicit DxvkMemory(Rc<DxvkResourceAllocation>&& allocation_)
    : allocation(std::move(allocation_)) { }

    DxvkMemory(DxvkMemory&& other) = default;
    DxvkMemory& operator = (DxvkMemory&& other) = default;

    ~DxvkMemory() = default;
    
    /**
     * \brief Memory object
     * 
     * This information is required when
     * binding memory to Vulkan objects.
     * \returns Memory object
     */
    VkDeviceMemory memory() const {
      return allocation ? allocation->m_memory : VK_NULL_HANDLE;
    }
    
    /**
     * \brief Buffer object
     *
     * Global buffer covering the entire memory allocation.
     * \returns Buffer object
     */
    VkBuffer buffer() const {
      return allocation ? allocation->m_buffer : VK_NULL_HANDLE;
    }

    /**
     * \brief Offset into device memory
     * 
     * This information is required when
     * binding memory to Vulkan objects.
     * \returns Offset into device memory
     */
    VkDeviceSize offset() const {
      return allocation
        ? allocation->m_address & DxvkPageAllocator::ChunkAddressMask
        : 0u;
    }
    
    /**
     * \brief Pointer to mapped data
     * 
     * \param [in] offset Byte offset
     * \returns Pointer to mapped data
     */
    void* mapPtr(VkDeviceSize offset) const {
      return allocation && allocation->m_mapPtr
        ? reinterpret_cast<char*>(allocation->m_mapPtr) + offset
        : nullptr;
    }

    /**
     * \brief Returns length of memory allocated
     * \returns Memory size
     */
    VkDeviceSize length() const {
      return allocation ? allocation->m_size : 0u;
    }

    /**
     * \brief Checks whether the memory slice is defined
     * 
     * \returns \c true if this slice points to actual device
     *          memory, and \c false if it is undefined.
     */
    explicit operator bool () const {
      return bool(allocation);
    }

    /**
     * \brief Queries global buffer usage flags
     * \returns Global buffer usage flags, if any
     */
    VkBufferUsageFlags getBufferUsage() const {
      return allocation && allocation->m_type
        ? allocation->m_type->bufferUsage
        : 0u;
    }

    Rc<DxvkResourceAllocation> allocation;
  };


  /**
   * \brief Memory allocator
   * 
   * Allocates device memory for Vulkan resources.
   * Memory objects will be destroyed automatically.
   */
  class DxvkMemoryAllocator {
    friend DxvkMemory;
    friend DxvkResourceAllocation;

    constexpr static uint64_t DedicatedChunkAddress = 1ull << 63u;

    constexpr static VkDeviceSize MinChunkSize =   4ull << 20;
    constexpr static VkDeviceSize MaxChunkSize = 256ull << 20;

    // Assume an alignment of 256 bytes. This is enough to satisfy all
    // buffer use cases, and matches our minimum allocation size.
    constexpr static VkDeviceSize GlobalBufferAlignment = 256u;
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
     * \returns Allocated memory
     */
    Rc<DxvkResourceAllocation> allocateMemory(
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
     * \returns Allocated memory
     */
    Rc<DxvkResourceAllocation> allocateDedicatedMemory(
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

    dxvk::mutex               m_mutex;
    dxvk::condition_variable  m_cond;

    uint32_t m_memTypeCount = 0u;
    uint32_t m_memHeapCount = 0u;

    std::array<DxvkMemoryType, VK_MAX_MEMORY_TYPES> m_memTypes = { };
    std::array<DxvkMemoryHeap, VK_MAX_MEMORY_HEAPS> m_memHeaps = { };

    uint32_t m_sparseMemoryTypes = 0u;

    std::array<uint32_t, 16> m_memTypesByPropertyFlags = { };

    DxvkResourceAllocationPool  m_allocationPool;

    dxvk::thread              m_worker;
    bool                      m_stopWorker = false;

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

    void freeDeviceMemory(
            DxvkMemoryType&       type,
            DxvkDeviceMemory      memory);

    void freeAllocation(
            DxvkResourceAllocation* allocation);

    uint32_t countEmptyChunksInPool(
      const DxvkMemoryPool&       pool) const;

    void freeEmptyChunksInHeap(
      const DxvkMemoryHeap&       heap,
            VkDeviceSize          allocationSize,
            high_resolution_clock::time_point time);

    void freeEmptyChunksInPool(
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

    Rc<DxvkResourceAllocation> createAllocation(
            DxvkMemoryType&       type,
            DxvkMemoryPool&       pool,
            VkDeviceSize          address,
            VkDeviceSize          size);

    Rc<DxvkResourceAllocation> createAllocation(
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

    void runWorker();

  };
  


  inline void DxvkResourceAllocation::free() {
    m_allocator->freeAllocation(this);
  }

}
