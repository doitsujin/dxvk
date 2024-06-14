#pragma once

#include "dxvk_adapter.h"

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
    VkDeviceMemory        memHandle  = VK_NULL_HANDLE;
    void*                 memPointer = nullptr;
    VkDeviceSize          memSize    = 0;
    VkMemoryPropertyFlags memFlags   = 0;
    float                 priority   = 0.0f;
  };

  
  /**
   * \brief Memory heap
   * 
   * Corresponds to a Vulkan memory heap and stores
   * its properties as well as allocation statistics.
   */
  struct DxvkMemoryHeap {
    VkMemoryHeap      properties;
    DxvkMemoryStats   stats;
  };


  /**
   * \brief Memory type
   * 
   * Corresponds to a Vulkan memory type and stores
   * memory chunks used to sub-allocate memory on
   * this memory type.
   */
  struct DxvkMemoryType {
    DxvkMemoryHeap*   heap;
    uint32_t          heapId;

    VkMemoryType      memType;
    uint32_t          memTypeId;

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
    
  private:
    
    DxvkMemoryAllocator*  m_alloc  = nullptr;
    DxvkMemoryChunk*      m_chunk  = nullptr;
    DxvkMemoryType*       m_type   = nullptr;
    VkDeviceMemory        m_memory = VK_NULL_HANDLE;
    VkDeviceSize          m_offset = 0;
    VkDeviceSize          m_length = 0;
    void*                 m_mapPtr = nullptr;
    
    void free();
    
  };


  /**
   * \brief Memory allocation flags
   *
   * Used to batch similar allocations into the same
   * set of chunks, which may help with fragmentation.
   */
  enum class DxvkMemoryFlag : uint32_t {
    Small             = 0,  ///< Small allocation
    GpuReadable       = 1,  ///< Medium-priority resource
    GpuWritable       = 2,  ///< High-priority resource
    Transient         = 3,  ///< Resource is short-lived
    IgnoreConstraints = 4,  ///< Ignore most allocation flags
  };

  using DxvkMemoryFlags = Flags<DxvkMemoryFlag>;
  
  
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
            DxvkDeviceMemory      memory,
            DxvkMemoryFlags       m_hints);
    
    ~DxvkMemoryChunk();

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
            VkDeviceSize          align,
            DxvkMemoryFlags       hints);
    
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

    /**
     * \brief Checks whether hints and flags of another chunk match
     * \param [in] other The chunk to compare to
     */
    bool isCompatible(const Rc<DxvkMemoryChunk>& other) const;

  private:
    
    struct FreeSlice {
      VkDeviceSize offset;
      VkDeviceSize length;
    };
    
    DxvkMemoryAllocator*  m_alloc;
    DxvkMemoryType*       m_type;
    DxvkDeviceMemory      m_memory;
    DxvkMemoryFlags       m_hints;
    
    std::vector<FreeSlice> m_freeList;

    bool checkHints(DxvkMemoryFlags hints) const;
    
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
  public:
    
    DxvkMemoryAllocator(DxvkDevice* device);
    ~DxvkMemoryAllocator();
    
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
     * \param [in] hints Memory hints
     * \returns Allocated memory slice
     */
    DxvkMemory alloc(
            DxvkMemoryRequirements            req,
            DxvkMemoryProperties              info,
            DxvkMemoryFlags                   hints);
    
    /**
     * \brief Queries memory stats
     * 
     * Returns the total amount of memory
     * allocated and used for a given heap.
     * \param [in] heap Heap index
     * \returns Memory stats for this heap
     */
    DxvkMemoryStats getMemoryStats(uint32_t heap) const {
      return m_memHeaps[heap].stats;
    }
    
  private:

    DxvkDevice*                                     m_device;
    VkPhysicalDeviceMemoryProperties                m_memProps;
    
    dxvk::mutex                                     m_mutex;
    std::array<DxvkMemoryHeap, VK_MAX_MEMORY_HEAPS> m_memHeaps;
    std::array<DxvkMemoryType, VK_MAX_MEMORY_TYPES> m_memTypes;

    VkDeviceSize                                    m_maxChunkSize;

    uint32_t m_sparseMemoryTypes = 0u;

    DxvkMemory tryAlloc(
      const DxvkMemoryRequirements&           req,
      const DxvkMemoryProperties&             info,
            DxvkMemoryFlags                   hints);
    
    DxvkMemory tryAllocFromType(
            DxvkMemoryType*                   type,
            VkDeviceSize                      size,
            VkDeviceSize                      align,
      const DxvkMemoryProperties&             info,
            DxvkMemoryFlags                   hints);
    
    DxvkDeviceMemory tryAllocDeviceMemory(
            DxvkMemoryType*                   type,
            VkDeviceSize                      size,
            DxvkMemoryProperties              info,
            DxvkMemoryFlags                   hints);
    
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
            DxvkMemoryFlags       hints) const;

    bool shouldFreeChunk(
      const DxvkMemoryType*       type,
      const Rc<DxvkMemoryChunk>&  chunk) const;

    bool shouldFreeEmptyChunks(
      const DxvkMemoryHeap*       heap,
            VkDeviceSize          allocationSize) const;

    void freeEmptyChunks(
      const DxvkMemoryHeap*       heap);

    uint32_t determineSparseMemoryTypes(
            DxvkDevice*           device) const;

    VkDeviceSize determineMaxChunkSize(
            DxvkDevice*           device) const;

    void logMemoryError(
      const VkMemoryRequirements& req) const;

    void logMemoryStats() const;

  };
  
}
