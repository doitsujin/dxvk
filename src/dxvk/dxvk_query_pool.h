#pragma once

#include "dxvk_query.h"

namespace dxvk {
  
  /**
   * \brief Query pool
   * 
   * Manages a Vulkan query pool. This is used
   * to allocate actual query objects for virtual
   * query objects.
   */
  class DxvkQueryPool : public RcObject {
    
  public:
    
    DxvkQueryPool(
      const Rc<vk::DeviceFn>& fn,
            VkQueryType       queryType,
            uint32_t          queryCount);
    
    ~DxvkQueryPool();
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    VkQueryPool      m_queryPool = VK_NULL_HANDLE;
    
    std::vector<Rc<DxvkQuery>> m_queries;
    
  };
  
}