#include "dxvk_query_manager.h"
#include "dxvk_query_pool.h"

namespace dxvk {

  DxvkQueryManager::DxvkQueryManager(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    
  }


  DxvkQueryManager::~DxvkQueryManager() {

  }


  DxvkQueryHandle DxvkQueryManager::allocQuery(
    const Rc<DxvkCommandList>&  cmd,
    const DxvkQueryRevision&    query) {
    const VkQueryType queryType = query.query->type();

    DxvkQueryHandle queryHandle = DxvkQueryHandle();
    Rc<DxvkQueryPool>& queryPool = this->getQueryPool(queryType);

    if (queryPool != nullptr)
      queryHandle = queryPool->allocQuery(query);
    
    if (queryHandle.queryPool == VK_NULL_HANDLE) {
      if (queryPool != nullptr)
        this->trackQueryPool(cmd, queryPool);
      
      queryPool = new DxvkQueryPool(m_vkd, queryType, MaxNumQueryCountPerPool);
      queryPool->reset(cmd);

      queryHandle = queryPool->allocQuery(query);
    }
    
    return queryHandle;
  }

  
  void DxvkQueryManager::enableQuery(
    const Rc<DxvkCommandList>&  cmd,
    const DxvkQueryRevision&    query) {
    m_activeQueries.push_back(query);

    if (m_activeTypes.test(query.query->type())) {
      DxvkQueryHandle handle = this->allocQuery(cmd, query);
      
      cmd->cmdBeginQuery(
        handle.queryPool,
        handle.queryId,
        handle.flags);
    }
  }

  
  void DxvkQueryManager::disableQuery(
    const Rc<DxvkCommandList>&  cmd,
    const DxvkQueryRevision&    query) {
    auto iter = m_activeQueries.begin();

    while (iter != m_activeQueries.end()) {
      if (iter->query    == query.query
       && iter->revision == query.revision)
        break;
      
      iter++;
    }
    
    if (iter != m_activeQueries.end()) {
      if (m_activeTypes.test(iter->query->type())) {
        DxvkQueryHandle handle = iter->query->getHandle();

        cmd->cmdEndQuery(
          handle.queryPool,
          handle.queryId);
      }
      
      m_activeQueries.erase(iter);
    }
  }


  void DxvkQueryManager::beginQueries(
    const Rc<DxvkCommandList>&  cmd,
          DxvkQueryTypeFlags    types) {
    m_activeTypes.set(types);

    for (const DxvkQueryRevision& query : m_activeQueries) {
      if (types.test(query.query->type())) {
        DxvkQueryHandle handle = this->allocQuery(cmd, query);
        
        cmd->cmdBeginQuery(
          handle.queryPool,
          handle.queryId,
          handle.flags);
      }
    }
  }

  
  void DxvkQueryManager::endQueries(
    const Rc<DxvkCommandList>&  cmd,
          DxvkQueryTypeFlags    types) {
    m_activeTypes.clr(types);

    for (const DxvkQueryRevision& query : m_activeQueries) {
      if (types.test(query.query->type())) {
        DxvkQueryHandle handle = query.query->getHandle();
        
        cmd->cmdEndQuery(
          handle.queryPool,
          handle.queryId);
      }
    }
  }


  void DxvkQueryManager::trackQueryPools(const Rc<DxvkCommandList>& cmd) {
    this->trackQueryPool(cmd, m_occlusion);
    this->trackQueryPool(cmd, m_pipeStats);
    this->trackQueryPool(cmd, m_timestamp);
  }


  void DxvkQueryManager::trackQueryPool(
    const Rc<DxvkCommandList>&  cmd,
    const Rc<DxvkQueryPool>&    pool) {
    if (pool != nullptr) {
      DxvkQueryRange range = pool->getActiveQueryRange();

      if (range.queryCount > 0)
        cmd->trackQueryRange(std::move(range));
    }
  }


  Rc<DxvkQueryPool>& DxvkQueryManager::getQueryPool(VkQueryType type) {
    switch (type) {
      case VK_QUERY_TYPE_OCCLUSION:
        return m_occlusion;
      
      case VK_QUERY_TYPE_PIPELINE_STATISTICS:
        return m_pipeStats;
      
      case VK_QUERY_TYPE_TIMESTAMP:
        return m_timestamp;

      default:
        throw DxvkError("DXVK: Invalid query type");
    }
  }

}