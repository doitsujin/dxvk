#pragma once

#include "dxvk_query_pool.h"

namespace dxvk {
  
  /**
   * \brief Query tracker
   */
  class DxvkQueryTracker {
    
  public:
    
    DxvkQueryTracker();
    ~DxvkQueryTracker();
    
    /**
     * \brief Adds a query range to track
     * \param [in] queryRange The query range
     */
    void trackQueryRange(DxvkQueryRange&& queryRange);
    
    /**
     * \brief Fetches query data
     * 
     * Retrieves query data from the query pools
     * and writes it back to the query objects.
     */
    void writeQueryData();
    
    /**
     * \brief Resets query tracker
     * 
     * Releases all query ranges from the tracker.
     * Call this after writing back the query data.
     */
    void reset();
    
  private:
    
    std::vector<DxvkQueryRange> m_queries;
    
  };
  
}