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
      const Rc<vk::DeviceFn>& vkd,
            VkQueryType       queryType);
    
    ~DxvkQueryPool();
    
    /**
     * \brief Query pool handle
     * \returns Query pool handle
     */
    VkQueryPool handle() const {
      return m_queryPool;
    }
    
    /**
     * \brief Query count
     * \returns Query count
     */
    uint32_t queryCount() const {
      return MaxNumQueryCountPerPool;
    }
    
    /**
     * \brief Allocates a Vulkan query
     * 
     * \param [in] revision Query revision
     * \returns The query ID and pool handle
     */
    DxvkQueryHandle allocQuery(
      const DxvkQueryRevision& revision);
    
    /**
     * \brief Writes back data for a range of queries
     * 
     * \param [in] queryIndex First query in the range
     * \param [in] queryCount Number of queries
     * \returns Query result status
     */
    VkResult getData(
            uint32_t          queryIndex,
            uint32_t          queryCount);
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    
    VkQueryType m_queryType;
    VkQueryPool m_queryPool = VK_NULL_HANDLE;
    
    std::array<DxvkQueryRevision, MaxNumQueryCountPerPool> m_queries;
    uint32_t                                               m_queryId = 0;
    
  };
  
}