#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  class DxvkDevice;

  /**
   * \brief Pipeline cache
   * 
   * Allows the Vulkan implementation to
   * re-use previously compiled pipelines.
   */
  class DxvkPipelineCache {
    
  public:
    
    DxvkPipelineCache(DxvkDevice* device);
    ~DxvkPipelineCache();
    
    /**
     * \brief Pipeline cache handle
     * \returns Pipeline cache handle
     */
    VkPipelineCache handle() const {
      return m_handle;
    }
    
  private:
    
    DxvkDevice*     m_device;
    VkPipelineCache m_handle = VK_NULL_HANDLE;
    
  };
  
}
