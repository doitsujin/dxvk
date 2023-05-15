#pragma once

#include <mutex>

#include "dxvk_buffer.h"
#include "dxvk_image.h"
#include "dxvk_sampler.h"

namespace dxvk {

  class DxvkContext;
  
  /**
   * \brief Unbound resources
   * 
   * Creates dummy resources that will be used
   * for descriptor sets when the client API did
   * not bind a compatible resource to a slot.
   */
  class DxvkUnboundResources {
    
  public:
    
    DxvkUnboundResources(DxvkDevice* dev);
    ~DxvkUnboundResources();
    
    /**
     * \brief Dummy buffer handle
     * 
     * Returns a handle to a buffer filled
     * with zeroes. Use for unbound vertex
     * and index buffers.
     * \returns Dummy buffer handle
     */
    VkBuffer bufferHandle();
    
    /**
     * \brief Dummy sampler descriptor
     * 
     * Points to a sampler which was created with
     * reasonable default values. Client APIs may
     * still require different behaviour.
     * \returns Dummy sampler descriptor
     */
    VkSampler samplerHandle();
    
  private:
    
    DxvkDevice*             m_device;

    std::atomic<VkSampler>  m_samplerHandle = { VK_NULL_HANDLE };
    std::atomic<VkBuffer>   m_bufferHandle  = { VK_NULL_HANDLE };

    dxvk::mutex             m_mutex;
    Rc<DxvkSampler>         m_sampler;
    Rc<DxvkBuffer>          m_buffer;
    
    Rc<DxvkSampler> createSampler();
    
    Rc<DxvkBuffer> createBuffer();
    
  };
  
}