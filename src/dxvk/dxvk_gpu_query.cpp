#include <algorithm>

#include "dxvk_cmdlist.h"
#include "dxvk_device.h"
#include "dxvk_gpu_query.h"

namespace dxvk {

  DxvkGpuQuery::DxvkGpuQuery(
    const Rc<vk::DeviceFn>&   vkd,
          VkQueryType         type,
          VkQueryControlFlags flags,
          uint32_t            index)
  : m_vkd(vkd), m_type(type), m_flags(flags),
    m_index(index), m_ended(false) {
    
  }
  
  
  DxvkGpuQuery::~DxvkGpuQuery() {
    if (m_handle.queryPool)
      m_handle.allocator->freeQuery(m_handle);
    
    for (DxvkGpuQueryHandle handle : m_handles)
      handle.allocator->freeQuery(handle);
  }


  bool DxvkGpuQuery::isIndexed() const {
    return m_type == VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT;
  }


  DxvkGpuQueryStatus DxvkGpuQuery::getData(DxvkQueryData& queryData) const {
    queryData = DxvkQueryData();

    if (!m_ended)
      return DxvkGpuQueryStatus::Invalid;
    
    // Empty begin/end pair
    if (!m_handle.queryPool)
      return DxvkGpuQueryStatus::Available;
    
    // Get query data from all associated handles
    DxvkGpuQueryStatus status = getDataForHandle(queryData, m_handle);

    for (size_t i = 0; i < m_handles.size()
        && status == DxvkGpuQueryStatus::Available; i++)
      status = getDataForHandle(queryData, m_handles[i]);
    
    // Treat non-precise occlusion queries as available
    // if we already know the result will be non-zero
    if ((status == DxvkGpuQueryStatus::Pending)
     && (m_type == VK_QUERY_TYPE_OCCLUSION)
     && !(m_flags & VK_QUERY_CONTROL_PRECISE_BIT)
     && (queryData.occlusion.samplesPassed))
      status = DxvkGpuQueryStatus::Available;
    
    return status;
  }


  void DxvkGpuQuery::begin(const Rc<DxvkCommandList>& cmd) {
    m_ended = false;

    cmd->trackGpuQuery(m_handle);
    m_handle = DxvkGpuQueryHandle();

    for (const auto& handle : m_handles)
      cmd->trackGpuQuery(handle);
    m_handles.clear();
  }

  
  void DxvkGpuQuery::end() {
    m_ended = true;
  }


  void DxvkGpuQuery::addQueryHandle(const DxvkGpuQueryHandle& handle) {
    if (m_handle.queryPool)
      m_handles.push_back(m_handle);
    
    m_handle = handle;
  }


  DxvkGpuQueryStatus DxvkGpuQuery::getDataForHandle(
          DxvkQueryData&      queryData,
    const DxvkGpuQueryHandle& handle) const {
    DxvkQueryData tmpData;

    // Wait for the query to be reset first
    VkResult result;
    
    if (handle.resetEvent) {
      result = m_vkd->vkGetEventStatus(
        m_vkd->device(), handle.resetEvent);
    
      if (result == VK_EVENT_RESET)
        return DxvkGpuQueryStatus::Pending;
      else if (result != VK_EVENT_SET)
        return DxvkGpuQueryStatus::Failed;
    }
    
    // Try to copy query data to temporary structure
    result = m_vkd->vkGetQueryPoolResults(m_vkd->device(),
      handle.queryPool, handle.queryId, 1,
      sizeof(DxvkQueryData), &tmpData,
      sizeof(DxvkQueryData), VK_QUERY_RESULT_64_BIT);
    
    if (result == VK_NOT_READY)
      return DxvkGpuQueryStatus::Pending;
    else if (result != VK_SUCCESS)
      return DxvkGpuQueryStatus::Failed;
    
    // Add numbers to the destination structure
    switch (m_type) {
      case VK_QUERY_TYPE_OCCLUSION:
        queryData.occlusion.samplesPassed += tmpData.occlusion.samplesPassed;
        break;
      
      case VK_QUERY_TYPE_TIMESTAMP:
        queryData.timestamp.time = tmpData.timestamp.time;
        break;
      
      case VK_QUERY_TYPE_PIPELINE_STATISTICS:
        queryData.statistic.iaVertices       += tmpData.statistic.iaVertices;
        queryData.statistic.iaPrimitives     += tmpData.statistic.iaPrimitives;
        queryData.statistic.vsInvocations    += tmpData.statistic.vsInvocations;
        queryData.statistic.gsInvocations    += tmpData.statistic.gsInvocations;
        queryData.statistic.gsPrimitives     += tmpData.statistic.gsPrimitives;
        queryData.statistic.clipInvocations  += tmpData.statistic.clipInvocations;
        queryData.statistic.clipPrimitives   += tmpData.statistic.clipPrimitives;
        queryData.statistic.fsInvocations    += tmpData.statistic.fsInvocations;
        queryData.statistic.tcsPatches       += tmpData.statistic.tcsPatches;
        queryData.statistic.tesInvocations   += tmpData.statistic.tesInvocations;
        queryData.statistic.csInvocations    += tmpData.statistic.csInvocations;
        break;
      
      case VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT:
        queryData.xfbStream.primitivesWritten += tmpData.xfbStream.primitivesWritten;
        queryData.xfbStream.primitivesNeeded  += tmpData.xfbStream.primitivesNeeded;
        break;
      
      default:
        Logger::err(str::format("DXVK: Unhandled query type: ", m_type));
        return DxvkGpuQueryStatus::Invalid;
    }
    
    return DxvkGpuQueryStatus::Available;
  }
  
  
  
  
  DxvkGpuQueryAllocator::DxvkGpuQueryAllocator(
          DxvkDevice*         device,
          VkQueryType         queryType,
          uint32_t            queryPoolSize)
  : m_device        (device),
    m_vkd           (device->vkd()),
    m_queryType     (queryType),
    m_queryPoolSize (queryPoolSize) {

  }

  
  DxvkGpuQueryAllocator::~DxvkGpuQueryAllocator() {
    for (DxvkGpuQueryHandle handle : m_handles) {
      m_vkd->vkDestroyEvent(m_vkd->device(),
        handle.resetEvent, nullptr);
    }

    for (VkQueryPool pool : m_pools) {
      m_vkd->vkDestroyQueryPool(
        m_vkd->device(), pool, nullptr);
    }
  }

  
  DxvkGpuQueryHandle DxvkGpuQueryAllocator::allocQuery() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_handles.size() == 0)
      this->createQueryPool();

    if (m_handles.size() == 0)
      return DxvkGpuQueryHandle();
    
    DxvkGpuQueryHandle result = m_handles.back();
    m_handles.pop_back();
    return result;
  }


  void DxvkGpuQueryAllocator::freeQuery(DxvkGpuQueryHandle handle) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_handles.push_back(handle);
  }

  
  void DxvkGpuQueryAllocator::createQueryPool() {
    VkQueryPoolCreateInfo info;
    info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.pNext      = nullptr;
    info.flags      = 0;
    info.queryType  = m_queryType;
    info.queryCount = m_queryPoolSize;
    info.pipelineStatistics = 0;

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

    if (m_vkd->vkCreateQueryPool(m_vkd->device(), &info, nullptr, &queryPool)) {
      Logger::err(str::format("DXVK: Failed to create query pool (", m_queryType, "; ", m_queryPoolSize, ")"));
      return;
    }

    m_pools.push_back(queryPool);

    VkEventCreateInfo eventInfo;
    eventInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    eventInfo.pNext = nullptr;
    eventInfo.flags = 0;
    
    for (uint32_t i = 0; i < m_queryPoolSize; i++) {
      VkEvent event = VK_NULL_HANDLE;

      if (!m_device->features().extHostQueryReset.hostQueryReset
       && m_vkd->vkCreateEvent(m_vkd->device(), &eventInfo, nullptr, &event) != VK_SUCCESS) {
        Logger::err("DXVK: Failed to create query reset event");
        return;
      }

      m_handles.push_back({ this, event, queryPool, i });
    }
  }




  DxvkGpuQueryPool::DxvkGpuQueryPool(DxvkDevice* device)
  : m_occlusion(device, VK_QUERY_TYPE_OCCLUSION,                     16384),
    m_statistic(device, VK_QUERY_TYPE_PIPELINE_STATISTICS,           1024),
    m_timestamp(device, VK_QUERY_TYPE_TIMESTAMP,                     1024),
    m_xfbStream(device, VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT, 1024) {
    
  }
  

  DxvkGpuQueryPool::~DxvkGpuQueryPool() {

  }

  
  DxvkGpuQueryHandle DxvkGpuQueryPool::allocQuery(VkQueryType type) {
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
        return DxvkGpuQueryHandle();
    }
  }




  DxvkGpuQueryManager::DxvkGpuQueryManager(DxvkGpuQueryPool& pool)
  : m_pool(&pool), m_activeTypes(0) {

  }

  
  DxvkGpuQueryManager::~DxvkGpuQueryManager() {

  }


  void DxvkGpuQueryManager::enableQuery(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkGpuQuery>&     query) {
    query->begin(cmd);

    m_activeQueries.push_back(query);

    if (m_activeTypes & getQueryTypeBit(query->type()))
      beginSingleQuery(cmd, query);
  }

  
  void DxvkGpuQueryManager::disableQuery(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkGpuQuery>&     query) {
    auto iter = std::find(
      m_activeQueries.begin(),
      m_activeQueries.end(),
      query);
    
    if (iter != m_activeQueries.end()) {
      if (m_activeTypes & getQueryTypeBit((*iter)->type()))
        endSingleQuery(cmd, query);
      m_activeQueries.erase(iter);
      
      query->end();
    }
  }


  void DxvkGpuQueryManager::writeTimestamp(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkGpuQuery>&     query) {
    DxvkGpuQueryHandle handle = m_pool->allocQuery(query->type());
    
    query->begin(cmd);
    query->addQueryHandle(handle);
    query->end();

    cmd->cmdResetQuery(
      handle.queryPool,
      handle.queryId,
      handle.resetEvent);
    
    cmd->cmdWriteTimestamp(
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      handle.queryPool,
      handle.queryId);
    
    cmd->trackResource<DxvkAccess::None>(query);
  }


  void DxvkGpuQueryManager::beginQueries(
    const Rc<DxvkCommandList>&  cmd,
          VkQueryType           type) {
    m_activeTypes |= getQueryTypeBit(type);

    for (size_t i = 0; i < m_activeQueries.size(); i++) {
      if (m_activeQueries[i]->type() == type)
        beginSingleQuery(cmd, m_activeQueries[i]);
    }
  }

    
  void DxvkGpuQueryManager::endQueries(
    const Rc<DxvkCommandList>&  cmd,
          VkQueryType           type) {
    m_activeTypes &= ~getQueryTypeBit(type);

    for (size_t i = 0; i < m_activeQueries.size(); i++) {
      if (m_activeQueries[i]->type() == type)
        endSingleQuery(cmd, m_activeQueries[i]);
    }
  }


  void DxvkGpuQueryManager::beginSingleQuery(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkGpuQuery>&     query) {
    DxvkGpuQueryHandle handle = m_pool->allocQuery(query->type());
    
    cmd->cmdResetQuery(
      handle.queryPool,
      handle.queryId,
      handle.resetEvent);
    
    if (query->isIndexed()) {
      cmd->cmdBeginQueryIndexed(
        handle.queryPool,
        handle.queryId,
        query->flags(),
        query->index());
    } else {
      cmd->cmdBeginQuery(
        handle.queryPool,
        handle.queryId,
        query->flags());
    }
    
    query->addQueryHandle(handle);
  }


  void DxvkGpuQueryManager::endSingleQuery(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkGpuQuery>&     query) {
    DxvkGpuQueryHandle handle = query->handle();
    
    if (query->isIndexed()) {
      cmd->cmdEndQueryIndexed(
        handle.queryPool,
        handle.queryId,
        query->index());
    } else {
      cmd->cmdEndQuery(
        handle.queryPool,
        handle.queryId);
    }

    cmd->trackResource<DxvkAccess::None>(query);
  }
  
  
  uint32_t DxvkGpuQueryManager::getQueryTypeBit(
          VkQueryType           type) {
    switch (type) {
      case VK_QUERY_TYPE_OCCLUSION:                     return 0x01;
      case VK_QUERY_TYPE_PIPELINE_STATISTICS:           return 0x02;
      case VK_QUERY_TYPE_TIMESTAMP:                     return 0x04;
      case VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT: return 0x08;
      default:                                          return 0;
    }
  }




  DxvkGpuQueryTracker::DxvkGpuQueryTracker() { }
  DxvkGpuQueryTracker::~DxvkGpuQueryTracker() { }
  

  void DxvkGpuQueryTracker::trackQuery(DxvkGpuQueryHandle handle) {
    if (handle.queryPool)
      m_handles.push_back(handle);
  }


  void DxvkGpuQueryTracker::reset() {
    for (DxvkGpuQueryHandle handle : m_handles)
      handle.allocator->freeQuery(handle);
    
    m_handles.clear();
  }

}