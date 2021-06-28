#pragma once

#include <mutex>
#include <vector>

#include "dxvk_resource.h"

namespace dxvk {

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
  struct DxvkGpuQueryHandle {
    DxvkGpuQueryAllocator* allocator  = nullptr;
    VkEvent                resetEvent = VK_NULL_HANDLE;
    VkQueryPool            queryPool  = VK_NULL_HANDLE;
    uint32_t               queryId    = 0;
  };


  /**
   * \brief Query object
   * 
   * Manages Vulkan queries that are sub-allocated
   * from larger query pools 
   */
  class DxvkGpuQuery : public DxvkResource {

  public:

    DxvkGpuQuery(
      const Rc<vk::DeviceFn>&   vkd,
            VkQueryType         type,
            VkQueryControlFlags flags,
            uint32_t            index);
    
    ~DxvkGpuQuery();

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
     * \brief Retrieves current handle
     * 
     * Note that the query handle will change
     * when calling \ref addQueryHandle.
     * \returns Current query handle
     */
    DxvkGpuQueryHandle handle() const {
      return m_handle;
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
     * \brief Checks whether query is indexed
     * \returns \c true for indexed query types
     */
    bool isIndexed() const;

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
            DxvkQueryData&      queryData) const;

    /**
     * \brief Begins query
     * 
     * Moves all current query handles to the given
     * command list and sets the query into active
     * state. No data can be retrieved while the
     * query is active.
     * \param [in] cmd Command list
     */
    void begin(
      const Rc<DxvkCommandList>& cmd);

    /**
     * \brief Ends query
     * 
     * Sets query into pending state. Calling
     * \c getData is legal after calling this.
     */
    void end();

    /**
     * \brief Adds a query handle to the query
     * 
     * The given query handle shall be used when
     * retrieving query data. A query can have
     * multiple handles attached.
     * \param [in] handle The query handle
     */
    void addQueryHandle(
      const DxvkGpuQueryHandle& handle);

  private:

    Rc<vk::DeviceFn>    m_vkd;

    VkQueryType         m_type;
    VkQueryControlFlags m_flags;
    uint32_t            m_index;
    bool                m_ended;

    DxvkGpuQueryHandle  m_handle;
    
    std::vector<DxvkGpuQueryHandle> m_handles;
    
    DxvkGpuQueryStatus getDataForHandle(
            DxvkQueryData&      queryData,
      const DxvkGpuQueryHandle& handle) const;

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
            DxvkDevice*         device,
            VkQueryType         queryType,
            uint32_t            queryPoolSize);
    
    ~DxvkGpuQueryAllocator();

    /**
     * \brief Allocates a query
     * 
     * If possible, this returns a free query
     * from an existing query pool. Otherwise,
     * a new query pool will be created.
     * \returns Query handle
     */
    DxvkGpuQueryHandle allocQuery();

    /**
     * \brief Recycles a query
     * 
     * Returns a query back to the allocator
     * so that it can be reused. The query
     * must not be in pending state.
     * \param [in] handle Query to reset
     */
    void freeQuery(DxvkGpuQueryHandle handle);

  private:

    DxvkDevice*       m_device;
    Rc<vk::DeviceFn>  m_vkd;
    VkQueryType       m_queryType;
    uint32_t          m_queryPoolSize;
    
    dxvk::mutex                     m_mutex;
    std::vector<DxvkGpuQueryHandle> m_handles;
    std::vector<VkQueryPool>        m_pools;

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
    DxvkGpuQueryHandle allocQuery(VkQueryType type);

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
      const Rc<DxvkGpuQuery>&     query);
    
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
      const Rc<DxvkGpuQuery>&     query);
    
    /**
     * \brief Signals a time stamp query
     * 
     * Timestamp queries are not scoped.
     * \param [in] cmd Command list
     * \param [in] query Query to allocate
     */
    void writeTimestamp(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkGpuQuery>&     query);

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

    DxvkGpuQueryPool*             m_pool;
    uint32_t                      m_activeTypes;
    std::vector<Rc<DxvkGpuQuery>> m_activeQueries;

    void beginSingleQuery(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkGpuQuery>&     query);

    void endSingleQuery(
      const Rc<DxvkCommandList>&  cmd,
      const Rc<DxvkGpuQuery>&     query);
    
    static uint32_t getQueryTypeBit(
            VkQueryType           type);

  };


  /**
   * \brief Query tracker
   * 
   * Returns queries to their allocators after
   * the command buffer has finished executing.
   */
  class DxvkGpuQueryTracker {

  public:

    DxvkGpuQueryTracker();
    ~DxvkGpuQueryTracker();
    
    /**
     * \param Tracks a query
     * \param [in] handle Query handle
     */
    void trackQuery(DxvkGpuQueryHandle handle);

    /**
     * \brief Recycles all tracked handles
     * 
     * Releases all tracked query handles
     * to their respective query allocator.
     */
    void reset();

  private:

    std::vector<DxvkGpuQueryHandle> m_handles;

  };
}