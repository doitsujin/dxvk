#pragma once

#include <vector>

#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;
  
  /**
   * \brief Descriptor info
   * 
   * Stores information that is required to
   * update a single resource descriptor.
   */
  union DxvkDescriptorInfo {
    VkDescriptorImageInfo  image;
    VkDescriptorBufferInfo buffer;
    VkBufferView           texelBuffer;
  };
  
  
  /**
   * \brief Descriptor pool
   * 
   * Wrapper around a Vulkan descriptor pool that
   * descriptor sets can be allocated from.
   */
  class DxvkDescriptorPool : public RcObject {
    
  public:
    
    DxvkDescriptorPool(
      const Rc<vk::DeviceFn>& vkd);
    ~DxvkDescriptorPool();
    
    /**
     * \brief Allocates a descriptor set
     * 
     * \param [in] layout Descriptor set layout
     * \returns The descriptor set
     */
    VkDescriptorSet alloc(
      VkDescriptorSetLayout layout);
    
    /**
     * \brief Resets descriptor set allocator
     * 
     * Destroys all descriptor sets and
     * resets the Vulkan descriptor pools.
     */
    void reset();
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    VkDescriptorPool m_pool;
    
  };


  /**
   * \brief Descriptor pool tracker
   * 
   * Tracks descriptor pools that are either full
   * or no longer needed by the DXVK context. The
   * command list will reset and recycle all pools
   * once it has completed execution on the GPU.
   */
  class DxvkDescriptorPoolTracker {

  public:

    DxvkDescriptorPoolTracker(DxvkDevice* device);
    ~DxvkDescriptorPoolTracker();

    /**
     * \brief Adds a descriptor pool to track
     * \param [in] pool The descriptor pool
     */
    void trackDescriptorPool(Rc<DxvkDescriptorPool> pool);
    
    /**
     * \brief Resets event tracker
     * 
     * Resets all tracked descriptor pools
     * and returns them to the device.
     */
    void reset();

  private:

    DxvkDevice* m_device;

    std::vector<Rc<DxvkDescriptorPool>> m_pools;

  };
  
}