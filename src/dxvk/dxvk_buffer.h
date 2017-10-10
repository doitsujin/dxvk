#pragma once

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
    VkDeviceSize bufferSize;
    
  };
  
  
  /**
   * \brief DXVK buffer
   * 
   * A simple buffer resource that stores linear data.
   */
  class DxvkBuffer : public DxvkResource {
    
  public:
    
    
    
  private:
    
    
    
  };
  
}