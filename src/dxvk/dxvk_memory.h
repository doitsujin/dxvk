#pragma once

#include "dxvk_adapter.h"

namespace dxvk {
  
  class DxvkMemoryAllocator;
  
  
  /**
   * \brief Memory slice
   */
  class DxvkMemory {
    
  public:
    
    DxvkMemory();
    DxvkMemory(
      DxvkMemoryAllocator*  alloc,
      VkDeviceMemory        memory,
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
     * \brief Offset from memory object
     * 
     * This information is required when
     * binding memory to Vulkan objects.
     * \returns Offset from memory object
     */
    VkDeviceSize offset() const {
      return 0;
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
    
    DxvkMemoryAllocator*  m_alloc  = nullptr;
    VkDeviceMemory        m_memory = VK_NULL_HANDLE;
    void*                 m_mapPtr = nullptr;
    
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
    
    VkDeviceMemory allocMemory(
      VkDeviceSize    blockSize,
      uint32_t        memoryType);
    
    void freeMemory(
      VkDeviceMemory  memory);
    
  };
  
}