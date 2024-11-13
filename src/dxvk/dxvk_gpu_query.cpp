#include <utility>

#include "dxvk_cmdlist.h"
#include "dxvk_device.h"
#include "dxvk_gpu_query.h"

namespace dxvk {

  void DxvkGpuQuery::free() {
    m_allocator->freeQuery(this);
  }




  DxvkQuery::DxvkQuery(
    const Rc<DxvkDevice>&             device,
          VkQueryType                 type,
          VkQueryControlFlags         flags,
          uint32_t                    index)
  : m_device(device), m_type(type), m_flags(flags), m_index(index) {

  }
  
  
  DxvkQuery::~DxvkQuery() {

  }


  DxvkGpuQueryStatus DxvkQuery::getData(DxvkQueryData& queryData) {
    queryData = DxvkQueryData();

    // Callers must ensure that no begin call is pending when
    // calling this. Given that, once the query is ended, we
    // know that no other thread will access query state.
    std::lock_guard lock(m_mutex);

    if (!m_ended)
      return DxvkGpuQueryStatus::Invalid;

    // Accumulate query data from all available queries
    DxvkGpuQueryStatus status = accumulateQueryDataLocked();

    // Treat non-precise occlusion queries as available
    // if we already know the result will be non-zero
    if ((status == DxvkGpuQueryStatus::Pending)
     && (m_type == VK_QUERY_TYPE_OCCLUSION)
     && !(m_flags & VK_QUERY_CONTROL_PRECISE_BIT)
     && (m_queryData.occlusion.samplesPassed))
      status = DxvkGpuQueryStatus::Available;

    // Write back accumulated query data if the result is useful
    if (status == DxvkGpuQueryStatus::Available)
      queryData = m_queryData;

    return status;
  }


  void DxvkQuery::begin() {
    std::lock_guard lock(m_mutex);
    m_queries.clear();
    m_queryData = { };
    m_ended = false;
  }


  void DxvkQuery::end() {
    std::lock_guard lock(m_mutex);
    m_ended = true;
  }


  void DxvkQuery::addGpuQuery(Rc<DxvkGpuQuery> query) {
    // Already accumulate available queries here in case
    // we already allocated a large number of queries
    std::lock_guard lock(m_mutex);

    if (m_queries.size() >= m_queries.MinCapacity)
      accumulateQueryDataLocked();

    m_queries.push_back(std::move(query));
  }


  DxvkGpuQueryStatus DxvkQuery::accumulateQueryDataForGpuQueryLocked(
    const Rc<DxvkGpuQuery>&           query) {
    auto vk = m_device->vkd();

    DxvkQueryData tmpData = { };

    // Try to copy query data to temporary structure
    std::pair<VkQueryPool, uint32_t> handle = query->getQuery();

    VkResult result = vk->vkGetQueryPoolResults(
      vk->device(), handle.first, handle.second, 1,
      sizeof(DxvkQueryData), &tmpData,
      sizeof(DxvkQueryData), VK_QUERY_RESULT_64_BIT);

    if (result == VK_NOT_READY)
      return DxvkGpuQueryStatus::Pending;
    else if (result != VK_SUCCESS)
      return DxvkGpuQueryStatus::Failed;

    // Add numbers to the destination structure
    switch (m_type) {
      case VK_QUERY_TYPE_OCCLUSION:
        m_queryData.occlusion.samplesPassed += tmpData.occlusion.samplesPassed;
        break;

      case VK_QUERY_TYPE_TIMESTAMP:
        m_queryData.timestamp.time = tmpData.timestamp.time;
        break;

      case VK_QUERY_TYPE_PIPELINE_STATISTICS:
        m_queryData.statistic.iaVertices       += tmpData.statistic.iaVertices;
        m_queryData.statistic.iaPrimitives     += tmpData.statistic.iaPrimitives;
        m_queryData.statistic.vsInvocations    += tmpData.statistic.vsInvocations;
        m_queryData.statistic.gsInvocations    += tmpData.statistic.gsInvocations;
        m_queryData.statistic.gsPrimitives     += tmpData.statistic.gsPrimitives;
        m_queryData.statistic.clipInvocations  += tmpData.statistic.clipInvocations;
        m_queryData.statistic.clipPrimitives   += tmpData.statistic.clipPrimitives;
        m_queryData.statistic.fsInvocations    += tmpData.statistic.fsInvocations;
        m_queryData.statistic.tcsPatches       += tmpData.statistic.tcsPatches;
        m_queryData.statistic.tesInvocations   += tmpData.statistic.tesInvocations;
        m_queryData.statistic.csInvocations    += tmpData.statistic.csInvocations;
        break;
      
      case VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT:
        m_queryData.xfbStream.primitivesWritten += tmpData.xfbStream.primitivesWritten;
        m_queryData.xfbStream.primitivesNeeded  += tmpData.xfbStream.primitivesNeeded;
        break;
      
      default:
        Logger::err(str::format("DXVK: Unhandled query type: ", m_type));
        return DxvkGpuQueryStatus::Invalid;
    }
    
    return DxvkGpuQueryStatus::Available;
  }


  DxvkGpuQueryStatus DxvkQuery::accumulateQueryDataLocked() {
    DxvkGpuQueryStatus status = DxvkGpuQueryStatus::Available;

    // Process available queries and return them to the
    // allocator if possible. This may help reduce the
    // number of Vulkan queries in flight.
    size_t queriesAvailable = 0;

    while (queriesAvailable < m_queries.size()) {
      status = accumulateQueryDataForGpuQueryLocked(m_queries[queriesAvailable]);

      if (status != DxvkGpuQueryStatus::Available)
        break;

      queriesAvailable += 1;
    }

    if (queriesAvailable) {
      for (size_t i = queriesAvailable; i < m_queries.size(); i++)
        m_queries[i - queriesAvailable] = m_queries[i];

      m_queries.resize(m_queries.size() - queriesAvailable);
    }

    return status;
  }
  
  
  
  
  DxvkGpuQueryAllocator::DxvkGpuQueryAllocator(
          DxvkDevice*                 device,
          VkQueryType                 queryType,
          uint32_t                    queryPoolSize)
  : m_device        (device),
    m_queryType     (queryType),
    m_queryPoolSize (queryPoolSize) {

  }

  
  DxvkGpuQueryAllocator::~DxvkGpuQueryAllocator() {
    auto vk = m_device->vkd();

    for (auto& p : m_pools) {
      vk->vkDestroyQueryPool(vk->device(), p.pool, nullptr);
      delete[] p.queries;
    }
  }

  
  Rc<DxvkGpuQuery> DxvkGpuQueryAllocator::allocQuery() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_free)
      createQueryPool();

    return std::exchange(m_free, m_free->m_next);
  }


  void DxvkGpuQueryAllocator::freeQuery(DxvkGpuQuery* query) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    query->m_next = std::exchange(m_free, query);
  }


  void DxvkGpuQueryAllocator::createQueryPool() {
    auto vk = m_device->vkd();

    VkQueryPoolCreateInfo info = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    info.queryType  = m_queryType;
    info.queryCount = m_queryPoolSize;

    if (m_queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
      info.pipelineStatistics
        = VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
        | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
        | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
        | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT
        | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT
        | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
        | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT
        | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT
        | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT
        | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT
        | VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
    }

    VkQueryPool queryPool = VK_NULL_HANDLE;

    if (vk->vkCreateQueryPool(vk->device(), &info, nullptr, &queryPool)) {
      Logger::err(str::format("DXVK: Failed to create query pool (", m_queryType, "; ", m_queryPoolSize, ")"));
      return;
    }

    auto& pool = m_pools.emplace_back();
    pool.pool = queryPool;
    pool.queries = new DxvkGpuQuery [m_queryPoolSize];

    for (uint32_t i = 0; i < m_queryPoolSize; i++) {
      auto& query = pool.queries[i];
      query.m_allocator = this;
      query.m_pool = queryPool;
      query.m_index = i;

      if (i + 1u < m_queryPoolSize)
        query.m_next = &pool.queries[i + 1u];
    }

    m_free = &pool.queries[0u];
  }




  DxvkGpuQueryPool::DxvkGpuQueryPool(DxvkDevice* device)
  : m_occlusion(device, VK_QUERY_TYPE_OCCLUSION,                     16384),
    m_statistic(device, VK_QUERY_TYPE_PIPELINE_STATISTICS,           1024),
    m_timestamp(device, VK_QUERY_TYPE_TIMESTAMP,                     1024),
    m_xfbStream(device, VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, 1024) {
    
  }
  

  DxvkGpuQueryPool::~DxvkGpuQueryPool() {

  }

  
  Rc<DxvkGpuQuery> DxvkGpuQueryPool::allocQuery(VkQueryType type) {
    switch (type) {
      case VK_QUERY_TYPE_OCCLUSION:
        return m_occlusion.allocQuery();
      case VK_QUERY_TYPE_PIPELINE_STATISTICS:
        return m_statistic.allocQuery();
      case VK_QUERY_TYPE_TIMESTAMP:
        return m_timestamp.allocQuery();
      case VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT:
        return m_xfbStream.allocQuery();
      default:
        Logger::err(str::format("DXVK: Unhandled query type: ", type));
        return nullptr;
    }
  }




  DxvkGpuQueryManager::DxvkGpuQueryManager(DxvkGpuQueryPool& pool)
  : m_pool(&pool) {

  }

  
  DxvkGpuQueryManager::~DxvkGpuQueryManager() {

  }


  void DxvkGpuQueryManager::enableQuery(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkQuery>&        query) {
    query->begin();

    uint32_t index = getQueryTypeIndex(query->type(), query->index());

    m_activeQueries[index].queries.push_back(query);

    if (m_activeTypes & getQueryTypeBit(query->type()))
      restartQueries(cmd, query->type(), query->index());
  }

  
  void DxvkGpuQueryManager::disableQuery(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkQuery>&        query) {
    uint32_t index = getQueryTypeIndex(query->type(), query->index());

    for (auto& q : m_activeQueries[index].queries) {
      if (q == query) {
        q = std::move(m_activeQueries[index].queries.back());
        m_activeQueries[index].queries.pop_back();
        break;
      }
    }

    if (m_activeTypes & getQueryTypeBit(query->type()))
      restartQueries(cmd, query->type(), query->index());

    query->end();
  }


  void DxvkGpuQueryManager::writeTimestamp(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkQuery>&        query) {
    Rc<DxvkGpuQuery> q = m_pool->allocQuery(query->type());

    query->begin();
    query->addGpuQuery(q);
    query->end();

    std::pair<VkQueryPool, uint32_t> handle = q->getQuery();

    cmd->resetQuery(handle.first, handle.second);

    cmd->cmdWriteTimestamp(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      handle.first, handle.second);

    cmd->track(std::move(q));
  }


  void DxvkGpuQueryManager::beginQueries(
    const Rc<DxvkCommandList>&  cmd,
          VkQueryType           type) {
    m_activeTypes |= getQueryTypeBit(type);

    if (likely(type != VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)) {
      restartQueries(cmd, type, 0);
    } else {
      for (uint32_t i = 0; i < 4; i++)
        restartQueries(cmd, type, i);
    }
  }

    
  void DxvkGpuQueryManager::endQueries(
    const Rc<DxvkCommandList>&  cmd,
          VkQueryType           type) {
    m_activeTypes &= ~getQueryTypeBit(type);

    if (likely(type != VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)) {
      restartQueries(cmd, type, 0);
    } else {
      for (uint32_t i = 0; i < 4; i++)
        restartQueries(cmd, type, i);
    }
  }


  void DxvkGpuQueryManager::restartQueries(
    const Rc<DxvkCommandList>&  cmd,
          VkQueryType           type,
          uint32_t              index) {
    auto& array = m_activeQueries[getQueryTypeIndex(type, index)];

    // End active GPU query for the given type and index
    if (array.gpuQuery) {
      auto handle = array.gpuQuery->getQuery();

      if (type == VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)
        cmd->cmdEndQueryIndexed(handle.first, handle.second, index);
      else
        cmd->cmdEndQuery(handle.first, handle.second);

      array.gpuQuery = nullptr;
    }

    // If the query type is still active, allocate, reset and begin
    // a new GPU query and assign it to all virtual queries.
    if ((m_activeTypes & getQueryTypeBit(type)) && !array.queries.empty()) {
      array.gpuQuery = m_pool->allocQuery(type);
      auto handle = array.gpuQuery->getQuery();

      // If any active occlusion query has the precise flag set, we need
      // to respect it, otherwise just use a regular occlusion query.
      VkQueryControlFlags flags = 0u;

      for (const auto& q : array.queries) {
        flags |= q->flags();
        q->addGpuQuery(array.gpuQuery);
      }

      // Actually reset and begin the query
      cmd->resetQuery(handle.first, handle.second);

      if (type == VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT)
        cmd->cmdBeginQueryIndexed(handle.first, handle.second, flags, index);
      else
        cmd->cmdBeginQuery(handle.first, handle.second, flags);

      cmd->track(array.gpuQuery);
    }
  }


  uint32_t DxvkGpuQueryManager::getQueryTypeBit(
          VkQueryType           type) {
    return 1u << getQueryTypeIndex(type, 0u);
  }


  uint32_t DxvkGpuQueryManager::getQueryTypeIndex(
          VkQueryType           type,
          uint32_t              index) {
    switch (type) {
      case VK_QUERY_TYPE_OCCLUSION:                     return 0u;
      case VK_QUERY_TYPE_PIPELINE_STATISTICS:           return 1u;
      case VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT: return 2u + index;
      default:                                          return 0u;
    }
  }

}