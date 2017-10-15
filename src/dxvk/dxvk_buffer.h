#pragma once

#include "dxvk_memory.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Buffer create info
   * 
   * The properties of a buffer that are
   * passed to \ref DxvkDevice::createBuffer
   */
  struct DxvkBufferCreateInfo {
    
    /// Size of the buffer, in bytes
    VkDeviceSize size;
    
    /// Buffer usage flags
    VkBufferUsageFlags usage;
    
  };
  
  
  /**
   * \brief DXVK buffer
   * 
   * A simple buffer resource that stores
   * linear data. Can be mapped to host
   * memory.
   */
  class DxvkBuffer : public DxvkResource {
    
  public:
    
    DxvkBuffer(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkBufferCreateInfo& createInfo,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    ~DxvkBuffer();
    
    /**
     * \brief Buffer handle
     * \returns Buffer handle
     */
    VkBuffer handle() const {
      return m_buffer;
    }
    
    /**
     * \brief Buffer properties
     * \returns Buffer properties
     */
    const DxvkBufferCreateInfo& info() const {
      return m_info;
    }
    
    /**
     * \brief Map pointer
     * 
     * If the buffer has been created on a host-visible
     * memory type, the buffer memory is mapped and can
     * be accessed by the host.
     * \returns Pointer to mapped memory region
     */
    void* mapPtr() const {
      return m_memory.mapPtr();
    }
    
  private:
    
    Rc<vk::DeviceFn>      m_vkd;
    DxvkBufferCreateInfo  m_info;
    DxvkMemory            m_memory;
    VkBuffer              m_buffer = VK_NULL_HANDLE;
    
  };
  
}