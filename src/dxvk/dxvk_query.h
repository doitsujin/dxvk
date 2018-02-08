#pragma once

#include "dxvk_limits.h"

namespace dxvk {
  
  /**
   * \brief DXVK query object
   * 
   * Creates a Vulkan query pool that acts as if it
   * were a single query. This approach is necessary
   * in order to keep evaluating the query across
   * multiple command submissions.
   */
  class DxvkQuery {
    
  public:
    
    DxvkQuery(
      const Rc<vk::DeviceFn>& vkd,
            VkQueryType       type);
    ~DxvkQuery();
    
    /**
     * \brief Query pool handle
     * \returns Query pool handle
     */
    VkQueryPool handle() const {
      return m_queryPool;
    }
    
  private:
    
    Rc<vk::DeviceFn> m_vkd       = nullptr;
    VkQueryType      m_type      = VK_QUERY_TYPE_END_RANGE;
    VkQueryPool      m_queryPool = VK_NULL_HANDLE;
    uint32_t         m_queryId   = 0;
    
  };
  
}