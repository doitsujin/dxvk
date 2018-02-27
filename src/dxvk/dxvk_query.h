#pragma once

#include <mutex>

#include "dxvk_limits.h"

namespace dxvk {
  
  /**
   * \brief Query status
   * 
   * Allows the application to query
   * the current status of the query. 
   */
  enum class DxvkQueryStatus : uint32_t {
    Reset     = 0,  ///< Query is reset
    Active    = 1,  ///< Query is being recorded
    Pending   = 2,  ///< Query has been recorded
    Available = 3,  ///< Query results can be retrieved
  };
  
  /**
   * \brief Occlusion query data
   * 
   * Stores the number of samples
   * that passes fragment tests.
   */
  struct DxvkQueryOcclusionData {
    uint64_t samplesPassed;
  };
  
  /**
   * \brief Timestamp data
   * 
   * Stores a GPU time stamp.
   */
  struct DxvkQueryTimestampData {
    uint64_t time;
  };
  
  /**
   * \brief Pipeline statistics
   * 
   * Stores the counters for
   * pipeline statistics queries.
   */
  struct DxvkQueryStatisticData {
    uint64_t iaVertices;
    uint64_t iaPrimitives;
    uint64_t vsInvocations;
    uint64_t gsInvocations;
    uint64_t gsPrimitives;
    uint64_t clipInvocations;
    uint64_t clipPrimitives;
    uint64_t fsInvocations;
    uint64_t tcsPatches;
    uint64_t tesInvocations;
    uint64_t csInvocations;
  };
  
  /**
   * \brief Query data
   * 
   * A union that stores query data. Select an
   * appropriate member based on the query type.
   */
  union DxvkQueryData {
    DxvkQueryOcclusionData occlusion;
    DxvkQueryTimestampData timestamp;
    DxvkQueryStatisticData statistic;
  };
  
  /**
   * \brief Query entry
   * 
   * Stores the pool handle and the
   * index of a single Vulkan query.
   */
  struct DxvkQueryHandle {
    VkQueryPool         queryPool = VK_NULL_HANDLE;
    uint32_t            queryId   = 0;
    VkQueryControlFlags flags     = 0;
  };
  
  /**
   * \brief Query object
   * 
   * Represents a single virtual query. Since queries
   * in Vulkan cannot be active across command buffer
   * submissions, we need to 
   */
  class DxvkQuery : public RcObject {
    
  public:
    
    DxvkQuery(
      VkQueryType         type,
      VkQueryControlFlags flags);
    ~DxvkQuery();
    
    /**
     * \brief Query type
     * \returns Query type
     */
    VkQueryType type() const {
      return m_type;
    }
    
    /**
     * \brief Query control flags
     * 
     * Flags that will be applied when
     * calling \c vkCmdBeginQuery.
     * \returns Query control flags
     */
    VkQueryControlFlags flags() const {
      return m_flags;
    }
    
    /**
     * \brief Resets the query object
     * 
     * Increments the revision number which will
     * be used to determine when query data becomes
     * available. All asynchronous query operations
     * will take the revision number as an argument.
     * \returns The new query revision number
     */
    uint32_t reset();
    
    /**
     * \brief Retrieves query data
     * 
     * \param [out] data Query data
     * \returns Query status
     */
    DxvkQueryStatus getData(
            DxvkQueryData& data);
    
    /**
     * \brief Gets current query handle
     * \returns The current query handle
     */
    DxvkQueryHandle getHandle();
    
    /**
     * \brief Begins recording the query
     * 
     * Sets internal query state to 'active'.
     * \param [in] revision Query version ID
     */
    void beginRecording(uint32_t revision);
    
    /**
     * \brief Ends recording the query
     * 
     * Sets internal query state to 'pending'.
     * \param [in] revision Query version ID
     */
    void endRecording(uint32_t revision);
    
    /**
     * \brief Increments internal query count
     * 
     * The internal query count is used to determine
     * when the query data is actually available.
     * \param [in] revision Query version ID
     * \param [in] handle The query handle
     */
    void associateQuery(
            uint32_t        revision,
            DxvkQueryHandle handle);
    
    /**
     * \brief Updates query data
     * 
     * Called by the command submission thread after
     * the Vulkan queries have been evaluated.
     * \param [in] revision Query version ID
     * \param [in] data Query data
     */
    void updateData(
            uint32_t       revision,
      const DxvkQueryData& data);
    
  private:
    
    const VkQueryType         m_type;
    const VkQueryControlFlags m_flags;
    
    std::mutex m_mutex;
    
    DxvkQueryStatus m_status   = DxvkQueryStatus::Available;
    DxvkQueryData   m_data     = {};
    DxvkQueryHandle m_handle;
    
    uint32_t m_queryIndex = 0;
    uint32_t m_queryCount = 0;
    uint64_t m_revision   = 0;
    
  };
  
  /**
   * \brief Query revision
   * 
   * Stores the query object and the
   * version ID for query operations.
   */
  struct DxvkQueryRevision {
    Rc<DxvkQuery> query;
    uint32_t      revision;
  };
  
}