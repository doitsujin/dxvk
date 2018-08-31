#pragma once

#include <vector>

#include "dxvk_cmdlist.h"
#include "dxvk_query.h"
#include "dxvk_query_pool.h"

namespace dxvk {

  using DxvkQueryTypeFlags = Flags<VkQueryType>;

  /**
   * \brief Query manager
   *
   * Manages Vulkan query pools
   * and the current query state.
   */
  class DxvkQueryManager {

  public:

    DxvkQueryManager(const Rc<vk::DeviceFn>& vkd);
    ~DxvkQueryManager();

    /**
     * \brief Allocates a Vulkan query
     * 
     * Creates a query pool of the correct type if
     * necessary, and allocates one  query from it.
     * \param [in] cmd The context's command list
     * \param [in] query The DXVK query revision
     * \returns Allocated query handle
     */
    DxvkQueryHandle allocQuery(
      const Rc<DxvkCommandList>&  cmd,
      const DxvkQueryRevision&    query);
    
    /**
     * \brief Enables a query
     * 
     * Starts tracking a query. Depending on the
     * query type, unterlying Vulkan queries will
     * begin and end on render pass boundaries.
     * \param [in] cmd The context's command list
     * \param [in] query The query to enable
     */
    void enableQuery(
      const Rc<DxvkCommandList>&  cmd,
      const DxvkQueryRevision&    query);
    
    /**
     * \brief Disables a query
     * 
     * Ends the query if it is currently active,
     * and stops tracking any further state changes.
     * \param [in] cmd The context's command list
     * \param [in] query The query to enable
     */
    void disableQuery(
      const Rc<DxvkCommandList>&  cmd,
      const DxvkQueryRevision&    query);
    
    /**
     * \brief Begins active queries
     * 
     * Creates a Vulkan query for each enabled
     * query of the given types and begins them.
     * \param [in] cmd The context's command list
     * \param [in] types Query types to begin
     */
    void beginQueries(
      const Rc<DxvkCommandList>&  cmd,
            DxvkQueryTypeFlags    types);
    
    /**
     * \brief Ends active queries
     * 
     * Ends active queries of the given types.
     * \param [in] cmd The context's command list
     * \param [in] types Query types to begin
     */
    void endQueries(
      const Rc<DxvkCommandList>&  cmd,
            DxvkQueryTypeFlags    types);
    
    /**
     * \brief Tracks query pools
     *
     * Adds all current non-empty query pools to
     * the query tracker of the given command list.
     * \param [in] cmd The context's command list
     */
    void trackQueryPools(
      const Rc<DxvkCommandList>&  cmd);

  private:

    const Rc<vk::DeviceFn> m_vkd;

    DxvkQueryTypeFlags m_activeTypes;

    Rc<DxvkQueryPool> m_occlusion;
    Rc<DxvkQueryPool> m_pipeStats;
    Rc<DxvkQueryPool> m_timestamp;

    std::vector<DxvkQueryRevision> m_activeQueries;

    void trackQueryPool(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkQueryPool>&    pool);
    
    Rc<DxvkQueryPool>& getQueryPool(
            VkQueryType           type);
    
  };

}