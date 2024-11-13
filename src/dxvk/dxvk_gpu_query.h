#pragma once

#include <atomic>
#include <mutex>
#include <list>
#include <vector>

#include "../util/util_small_vector.h"

#include "dxvk_include.h"

namespace dxvk {

  class DxvkDevice;
  class DxvkCommandList;

  class DxvkGpuQueryPool;
  class DxvkGpuQueryAllocator;

  /**
   * \brief Query status
   * 
   * Reports whether a query is in
   * signaled or unsignaled state.
   */
  enum class DxvkGpuQueryStatus : uint32_t {
    Invalid   = 0,
    Pending   = 1,
    Available = 2,
    Failed    = 3,
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
   * \brief Transform feedback stream query
   * 
   * Stores the number of primitives written to the
   * buffer, as well as the number of primitives
   * generated. The latter can be used to check for
   * overflow.
   */
  struct DxvkQueryXfbStreamData {
    uint64_t primitivesWritten;
    uint64_t primitivesNeeded;
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
    DxvkQueryXfbStreamData xfbStream;
  };


  /**
   * \brief Query handle
   * 
   * Stores the query allocator, as well as
   * the actual pool and query index. Since
   * query pools have to be reset on the GPU,
   * this also comes with a reset event.
   */
  class DxvkGpuQuery {
    friend class DxvkGpuQueryAllocator;
  public:

    /**
     * \brief Increments query reference count
     */
    force_inline void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements query reference count
     * Returns the query to its allocator if necessary.
     */
    force_inline void decRef() {
      if (m_refCount.fetch_sub(1u, std::memory_order_release) == 1u)
        free();
    }

    /**
     * \brief Retrieves query pool and index
     * \returns Query pool handle and query index
     */
    std::pair<VkQueryPool, uint32_t> getQuery() const {
      return std::make_pair(m_pool, m_index);
    }

  private:

    DxvkGpuQueryAllocator*  m_allocator = nullptr;
    DxvkGpuQuery*           m_next      = nullptr;

    VkQueryPool             m_pool      = VK_NULL_HANDLE;
    uint32_t                m_index     = 0u;

    std::atomic<uint32_t>   m_refCount  = { 0u };

    void free();

  };


  /**
   * \brief Virtual query object
   *
   * References an arbitrary number of Vulkan queries to
   * get feedback from the GPU. Vulkan queries can be used
   * by multiple virtual queries in case of overlap.
   */
  class DxvkQuery {
    friend class DxvkGpuQueryManager;
  public:

    DxvkQuery(
      const Rc<DxvkDevice>&             device,
            VkQueryType                 type,
            VkQueryControlFlags         flags,
            uint32_t                    index);
    
    ~DxvkQuery();

    /**
     * \brief Increments reference count
     */
    force_inline void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    /**
     * \brief Decrements reference count
     */
    force_inline void decRef() {
      if (m_refCount.fetch_sub(1u, std::memory_order_release) == 1u)
        delete this;
    }

    /**
     * \brief Query type
     * \returns Query type
     */
    VkQueryType type() const {
      return m_type;
    }

    /**
     * \brief Query control flags
     * \returns Query control flags
     */
    VkQueryControlFlags flags() const {
      return m_flags;
    }

    /**
     * \brief Query index
     * 
     * Only valid for indexed query types.
     * For non-zero values, indexed query
     * functions must be used.
     * \returns Query index
     */
    uint32_t index() const {
      return m_index;
    }

    /**
     * \brief Retrieves query data
     * 
     * If all query data is available, this will
     * return \c DxvkGpuQueryStatus::Signaled, and
     * the destination structure will be filled
     * with the data retrieved from all associated
     * query handles.
     * \param [out] queryData Query data
     * \returns Current query status
     */
    DxvkGpuQueryStatus getData(
            DxvkQueryData&      queryData);

    /**
     * \brief Begins query
     *
     * Invalidates previously retrieved data.
     */
    void begin();

    /**
     * \brief Ends query
     *
     * Sets query into pending state. Calling
     * \c getData is legal after calling this.
     */
    void end();

  private:

    std::atomic<uint32_t> m_refCount = { 0u };

    Rc<DxvkDevice>      m_device;

    VkQueryType         m_type  = VK_QUERY_TYPE_MAX_ENUM;
    VkQueryControlFlags m_flags = 0u;
    uint32_t            m_index = 0u;
    bool                m_ended = false;

    sync::Spinlock      m_mutex;
    DxvkQueryData       m_queryData = { };

    small_vector<Rc<DxvkGpuQuery>, 8> m_queries;

    DxvkGpuQueryStatus accumulateQueryDataForGpuQueryLocked(
      const Rc<DxvkGpuQuery>&           query);

    DxvkGpuQueryStatus accumulateQueryDataLocked();

    void addGpuQuery(
            Rc<DxvkGpuQuery>            query);

  };


  /**
   * \brief Query allocator
   * 
   * Creates query pools and allocates
   * queries for a single query type. 
   */
  class DxvkGpuQueryAllocator {

  public:

    DxvkGpuQueryAllocator(
            DxvkDevice*                 device,
            VkQueryType                 queryType,
            uint32_t                    queryPoolSize);

    ~DxvkGpuQueryAllocator();

    /**
     * \brief Allocates a query
     * 
     * If possible, this returns a free query from an existing
     * query pool. Otherwise, a new query pool will be created.
     * \returns Query handle
     */
    Rc<DxvkGpuQuery> allocQuery();

    /**
     * \brief Recycles a query
     * 
     * Returns a query back to the allocator so that it can be
     * reused. The query must not be in pending state.
     * \param [in] query Query object to recycle
     */
    void freeQuery(
            DxvkGpuQuery*               query);

  private:

    struct Pool {
      VkQueryPool   pool    = VK_NULL_HANDLE;
      DxvkGpuQuery* queries = nullptr;
    };

    DxvkDevice*       m_device        = nullptr;
    VkQueryType       m_queryType     = VK_QUERY_TYPE_MAX_ENUM;
    uint32_t          m_queryPoolSize = 0u;

    dxvk::mutex       m_mutex;
    std::list<Pool>   m_pools;

    DxvkGpuQuery*     m_free = nullptr;

    void createQueryPool();

  };


  /**
   * \brief Query pool
   * 
   * Small wrapper class that manages query
   * allocators for all supported query types,
   */
  class DxvkGpuQueryPool {

  public:

    DxvkGpuQueryPool(DxvkDevice* device);
    
    ~DxvkGpuQueryPool();
    
    /**
     * \brief Allocates a single query
     * 
     * \param [in] type Query type
     * \returns Handle to the allocated query
     */
    Rc<DxvkGpuQuery> allocQuery(VkQueryType type);

  private:

    DxvkGpuQueryAllocator m_occlusion;
    DxvkGpuQueryAllocator m_statistic;
    DxvkGpuQueryAllocator m_timestamp;
    DxvkGpuQueryAllocator m_xfbStream;

  };


  /**
   * \brief Query manager
   * 
   * Keeps track of enabled and disabled queries
   * and assigns Vulkan queries to them as needed.
   */
  class DxvkGpuQueryManager {
    constexpr static uint32_t MaxQueryTypes = 6u;
  public:

    DxvkGpuQueryManager(DxvkGpuQueryPool& pool);
    
    ~DxvkGpuQueryManager();

    /**
     * \brief Enables a query
     * 
     * This will also immediately begin the
     * query in case the query type is active.
     * \param [in] cmd Command list
     * \param [in] query Query to allocate
     */
    void enableQuery(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkQuery>&        query);
    
    /**
     * \brief Disables a query
     * 
     * This will also immediately end the
     * query in case the query type is active.
     * \param [in] cmd Command list
     * \param [in] query Query to allocate
     */
    void disableQuery(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkQuery>&        query);
    
    /**
     * \brief Signals a time stamp query
     * 
     * Timestamp queries are not scoped.
     * \param [in] cmd Command list
     * \param [in] query Query to allocate
     */
    void writeTimestamp(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkQuery>&        query);

    /**
     * \brief Begins queries of a given type
     * 
     * Makes a query type \e active. Begins
     * all enabled queries of this type.
     * \param [in] cmd Command list
     * \param [in] type Query type
     */
    void beginQueries(
      const Rc<DxvkCommandList>&  cmd,
            VkQueryType           type);
    
    /**
     * \brief Ends queries of a given type
     * 
     * Makes a query type \e inactive. Ends
     * all enabled queries of this type.
     * \param [in] cmd Command list
     * \param [in] type Query type
     */
    void endQueries(
      const Rc<DxvkCommandList>&  cmd,
            VkQueryType           type);

  private:

    struct QuerySet {
      Rc<DxvkGpuQuery>            gpuQuery;
      std::vector<Rc<DxvkQuery>>  queries;
    };

    DxvkGpuQueryPool*             m_pool        = nullptr;
    uint32_t                      m_activeTypes = 0u;

    std::array<QuerySet, MaxQueryTypes> m_activeQueries = { };

    void restartQueries(
      const Rc<DxvkCommandList>&  cmd,
            VkQueryType           type,
            uint32_t              index);

    static uint32_t getQueryTypeBit(
            VkQueryType           type);

    static uint32_t getQueryTypeIndex(
            VkQueryType           type,
            uint32_t              index);

  };

}
