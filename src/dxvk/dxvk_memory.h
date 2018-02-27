#pragma once

#include "dxvk_adapter.h"

namespace dxvk {
  
  class DxvkMemoryHeap;
  class DxvkMemoryChunk;
  class DxvkMemoryAllocator;
  
  
  /**
   * \brief Memory slice
   * 
   * Represents a slice of memory that has
   * been sub-allocated from a bigger chunk.
   */
  class DxvkMemory {
    
  public:
    
    DxvkMemory();
    DxvkMemory(
      DxvkMemoryChunk*  chunk,
      DxvkMemoryHeap*   heap,
      VkDeviceMemory    memory,
      VkDeviceSize      offset,
      VkDeviceSize      length,
      void*             mapPtr);
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
    
  private:
    
    DxvkMemoryChunk*  m_chunk  = nullptr;
    DxvkMemoryHeap*   m_heap   = nullptr;
    VkDeviceMemory    m_memory = VK_NULL_HANDLE;
    VkDeviceSize      m_offset = 0;
    VkDeviceSize      m_length = 0;
    void*             m_mapPtr = nullptr;
    
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
            DxvkMemoryHeap* heap,
            VkDeviceMemory  memory,
            void*           mapPtr,
            VkDeviceSize    size);
    
    ~DxvkMemoryChunk();
    
    /**
     * \brief Allocates memory from the chunk
     * 
     * On failure, this returns a slice with
     * \c VK_NULL_HANDLE as the memory handle.
     * \param [in] size Number of bytes to allocate
     * \param [in] align Required alignment
     * \returns The allocated memory slice
     */
    DxvkMemory alloc(
            VkDeviceSize size,
            VkDeviceSize align);
    
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
    
  private:
    
    struct FreeSlice {
      VkDeviceSize offset;
      VkDeviceSize length;
    };
    
    DxvkMemoryHeap* const m_heap;
    VkDeviceMemory  const m_memory;
    void*           const m_mapPtr;
    VkDeviceSize    const m_size;
    size_t m_delta = 0;
    std::vector<FreeSlice> m_freeList;
    
  };
  
  
  /**
   * \brief Memory heap
   * 
   * Implements a memory allocator for a single
   * memory type. This class is thread-safe.
   */
  class DxvkMemoryHeap : public RcObject {
    friend class DxvkMemory;
    friend class DxvkMemoryChunk;
  public:
    
    DxvkMemoryHeap(
      const Rc<vk::DeviceFn>    vkd,
            uint32_t            memTypeId,
            VkMemoryType        memType);
    
    DxvkMemoryHeap             (DxvkMemoryHeap&&) = delete;
    DxvkMemoryHeap& operator = (DxvkMemoryHeap&&) = delete;
    
    ~DxvkMemoryHeap();
    
    /**
     * \brief Allocates memory from the heap
     * 
     * Unless the requested allocation size is big
     * enough to justify a dedicated device allocation,
     * this will try to sub-allocate the block from an
     * existing chunk and create new chunks as necessary.
     * \param [in] size Amount of memory to allocate
     * \param [in] align Alignment requirements
     * \returns The allocated memory slice
     */
    DxvkMemory alloc(
            VkDeviceSize size,
            VkDeviceSize align);
    
  private:
    
    const Rc<vk::DeviceFn>           m_vkd;
    const uint32_t                   m_memTypeId;
    const VkMemoryType               m_memType;
    const VkDeviceSize               m_chunkSize = 16 * 1024 * 1024;
    
    std::mutex                       m_mutex;
    std::vector<Rc<DxvkMemoryChunk>> m_chunks;
    
    VkDeviceMemory allocDeviceMemory(
            VkDeviceSize    memorySize);
    
    void freeDeviceMemory(
            VkDeviceMemory  memory);
    
    void* mapDeviceMemory(
            VkDeviceMemory  memory);
    
    void free(
            DxvkMemoryChunk*  chunk,
            VkDeviceSize      offset,
            VkDeviceSize      length);
    
  };
  
  
  /**
   * \brief Memory allocator
   * 
   * Allocates device memory for Vulkan resources.
   * Memory objects will be destroyed automatically.
   */
  class DxvkMemoryAllocator : public RcObject {
    friend class DxvkMemory;
  public:
    
    DxvkMemoryAllocator(
      const Rc<DxvkAdapter>&  adapter,
      const Rc<vk::DeviceFn>& vkd);
    ~DxvkMemoryAllocator();
    
    /**
     * \brief Allocates device memory
     * 
     * \param [in] req Memory requirements
     * \param [in] flats Memory type flags
     * \returns Allocated memory slice
     */
    DxvkMemory alloc(
      const VkMemoryRequirements& req,
      const VkMemoryPropertyFlags flags);
    
  private:
    
    const Rc<vk::DeviceFn>                 m_vkd;
    const VkPhysicalDeviceMemoryProperties m_memProps;
    
    std::array<Rc<DxvkMemoryHeap>, VK_MAX_MEMORY_TYPES> m_heaps;
    
    DxvkMemory tryAlloc(
      const VkMemoryRequirements& req,
      const VkMemoryPropertyFlags flags);
    
  };
  
}