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
     * Returns a handle to a buffer filled with zeroes.
     * Use for unbound transform feedback buffers only.
     * \returns Dummy buffer handle
     */
    DxvkResourceBufferInfo bufferInfo();

    /**
     * \brief Dummy sampler object
     * 
     * Points to a sampler which was created with
     * reasonable default values. Client APIs may
     * still require different behaviour.
     * \returns Dummy sampler
     */
    DxvkSamplerDescriptor samplerInfo();

  private:
    
    DxvkDevice*             m_device;

    std::atomic<bool>       m_bufferCreated = { false };
    std::atomic<bool>       m_samplerCreated = { false };

    dxvk::mutex             m_mutex;
    Rc<DxvkSampler>         m_sampler;
    Rc<DxvkBuffer>          m_buffer;
    
    Rc<DxvkSampler> createSampler();
    
    Rc<DxvkBuffer> createBuffer();
    
  };
  
}
