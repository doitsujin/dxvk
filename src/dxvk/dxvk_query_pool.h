#pragma once

#include <vector>

#include "dxvk_query.h"

namespace dxvk {
  
  class DxvkCommandList;
  class DxvkQueryPool;
  
  /**
   * \brief Query range
   */
  struct DxvkQueryRange {
    Rc<DxvkQueryPool> queryPool;
    
    uint32_t queryIndex = 0;
    uint32_t queryCount = 0;
  };
  
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
            VkQueryType       queryType,
            uint32_t          queryCount);
    
    ~DxvkQueryPool();
    
    /**
     * \brief Query pool handle
     * \returns Query pool handle
     */
    VkQueryPool handle() const {
      return m_queryPool;
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
    
    /**
     * \brief Resets query pool
     * 
     * Resets the Vulkan query pool itself, as
     * well as the the internal query allocator.
     * \param [in] cmd Command list
     */
    void reset(
      const Rc<DxvkCommandList>& cmd);
    
    /**
     * \brief Retrieves active query range
     * 
     * This will also move the beginning of the
     * new active query range to the end of the
     * current active query range.
     * \returns Active query range
     */
    DxvkQueryRange getActiveQueryRange();
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    
    uint32_t    m_queryCount;
    VkQueryType m_queryType;
    VkQueryPool m_queryPool = VK_NULL_HANDLE;
    
    std::vector<DxvkQueryRevision> m_queries;
    
    uint32_t m_queryRangeOffset = 0;
    uint32_t m_queryRangeLength = 0;
    
  };
  
}