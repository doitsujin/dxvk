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
    VkBuffer              buffer     = VK_NULL_HANDLE;
    VkDeviceMemory        memHandle  = VK_NULL_HANDLE;
    void*                 memPointer = nullptr;
    VkDeviceSize          memSize    = 0;
  };

  
  /**
   * \brief Memory heap
   * 
   * Corresponds to a Vulkan memory heap and stores
   * its properties as well as allocation statistics.
   */
  struct DxvkMemoryHeap {
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
    DxvkMemoryHeap*   heap          = nullptr;
    uint32_t          heapId        = 0u;

    VkMemoryType      memType       = { };
    uint32_t          memTypeId     = 0u;

    DxvkMemoryStats   stats         = { };

    VkDeviceSize      chunkSize     = 0u;
    VkBufferUsageFlags bufferUsage  = 0u;

    std::vector<Rc<DxvkMemoryChunk>> chunks;
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
      DxvkMemoryChunk*      chunk,
      DxvkMemoryType*       type,
      VkBuffer              buffer,
      VkDeviceMemory        memory,
      VkDeviceSize          offset,
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
      return m_offset;
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
    DxvkMemoryChunk*      m_chunk  = nullptr;
    DxvkMemoryType*       m_type   = nullptr;
    VkBuffer              m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory        m_memory = VK_NULL_HANDLE;
    VkDeviceSize          m_offset = 0;
    VkDeviceSize          m_length = 0;
    void*                 m_mapPtr = nullptr;
    
    void free();
    
  };
  
  
  /**
   * \brief Memory chunk
   * 
   * A single chunk of memory that provides a
   * sub-allocator. This is not thread-safe.
   */
  class DxvkMemoryChunk : public RcObject {
    
  public:
    
    DxvkMemoryChunk(
            DxvkMemoryAllocator*  alloc,
            DxvkMemoryType*       type,
            DxvkDeviceMemory      memory);
    
    ~DxvkMemoryChunk();

    /**
     * \brief Queries chunk size
     * \returns Chunk size
     */
    VkDeviceSize size() const {
      return m_memory.memSize;
    }

    /**
     * \brief Allocates memory from the chunk
     * 
     * On failure, this returns a slice with
     * \c VK_NULL_HANDLE as the memory handle.
     * \param [in] flags Requested memory type flags
     * \param [in] size Number of bytes to allocate
     * \param [in] align Required alignment
     * \param [in] hints Memory category
     * \returns The allocated memory slice
     */
    DxvkMemory alloc(
            VkMemoryPropertyFlags flags,
            VkDeviceSize          size,
            VkDeviceSize          align);
    
    /**
     * \brief Frees memory
     * 
     * Returns a slice back to the chunk.
     * Called automatically when a memory
     * slice runs out of scope.
     * \param [in] offset Slice offset
     * \param [in] length Slice length
     */
    void free(
            VkDeviceSize  offset,
            VkDeviceSize  length);

    /**
     * \brief Checks whether the chunk is being used
     * \returns \c true if there are no allocations left
     */
    bool isEmpty() const;

  private:
    
    DxvkMemoryAllocator*  m_alloc;
    DxvkMemoryType*       m_type;
    DxvkDeviceMemory      m_memory;

    DxvkPageAllocator     m_pageAllocator;
    DxvkPoolAllocator     m_poolAllocator;

    void mapChunk();

    void unmapChunk();

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

    constexpr static VkDeviceSize SmallAllocationThreshold = 256 << 10;

    constexpr static VkDeviceSize MinChunkSize =   4ull << 20;
    constexpr static VkDeviceSize MaxChunkSize = 256ull << 20;
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
     * \param [in] req Memory requirements
     * \param [in] info Memory properties
     * \returns Allocated memory slice
     */
    DxvkMemory alloc(
            DxvkMemoryRequirements            req,
            DxvkMemoryProperties              info);
    
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

    DxvkDevice*                                     m_device;
    VkPhysicalDeviceMemoryProperties                m_memProps;
    
    dxvk::mutex                                     m_mutex;
    std::array<DxvkMemoryHeap, VK_MAX_MEMORY_HEAPS> m_memHeaps = { };
    std::array<DxvkMemoryType, VK_MAX_MEMORY_TYPES> m_memTypes = { };

    uint32_t m_sparseMemoryTypes = 0u;

    DxvkMemory tryAlloc(
      const DxvkMemoryRequirements&           req,
      const DxvkMemoryProperties&             info);
    
    DxvkMemory tryAllocFromType(
            DxvkMemoryType*                   type,
            VkDeviceSize                      size,
            VkDeviceSize                      align,
      const DxvkMemoryProperties&             info);
    
    DxvkDeviceMemory tryAllocDeviceMemory(
            DxvkMemoryType*                   type,
            VkDeviceSize                      size,
            DxvkMemoryProperties              info,
            bool                              isChunk);
    
    void free(
      const DxvkMemory&           memory);
    
    void freeChunkMemory(
            DxvkMemoryType*       type,
            DxvkMemoryChunk*      chunk,
            VkDeviceSize          offset,
            VkDeviceSize          length);
    
    void freeDeviceMemory(
            DxvkMemoryType*       type,
            DxvkDeviceMemory      memory);
    
    VkDeviceSize pickChunkSize(
            uint32_t              memTypeId,
            VkDeviceSize          requiredSize) const;

    void adjustChunkSize(
            uint32_t              memTypeId,
            VkDeviceSize          allocatedSize);

    bool shouldFreeChunk(
      const DxvkMemoryType*       type,
      const Rc<DxvkMemoryChunk>&  chunk) const;

    bool shouldFreeEmptyChunks(
            uint32_t              heapIndex,
            VkDeviceSize          allocationSize) const;

    void freeEmptyChunks(
      const DxvkMemoryHeap*       heap);

    uint32_t determineSparseMemoryTypes(
            DxvkDevice*           device) const;

    void determineBufferUsageFlagsPerMemoryType();

    void logMemoryError(
      const VkMemoryRequirements& req) const;

    void logMemoryStats() const;

  };
  
}
