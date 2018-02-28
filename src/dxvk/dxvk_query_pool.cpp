#include "dxvk_cmdlist.h"
#include "dxvk_query_pool.h"

namespace dxvk {
  
  DxvkQueryPool::DxvkQueryPool(
    const Rc<vk::DeviceFn>& vkd,
          VkQueryType       queryType,
          uint32_t          queryCount)
  : m_vkd(vkd), m_queryCount(queryCount), m_queryType(queryType) {
    m_queries.resize(queryCount);
    
    VkQueryPoolCreateInfo info;
    info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.pNext      = nullptr;
    info.flags      = 0;
    info.queryType  = queryType;
    info.queryCount = queryCount;
    info.pipelineStatistics = 0;
    
    if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
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
    
    if (m_vkd->vkCreateQueryPool(m_vkd->device(), &info, nullptr, &m_queryPool) != VK_SUCCESS)
      Logger::err("DxvkQueryPool: Failed to create query pool");
  }
  
  
  DxvkQueryPool::~DxvkQueryPool() {
    m_vkd->vkDestroyQueryPool(
      m_vkd->device(), m_queryPool, nullptr);
  }
  
  
  DxvkQueryHandle DxvkQueryPool::allocQuery(const DxvkQueryRevision& query) {
    const uint32_t queryIndex = m_queryRangeOffset + m_queryRangeLength;
    
    if (queryIndex >= m_queryCount)
      return DxvkQueryHandle();
    
    DxvkQueryHandle result;
    result.queryPool = m_queryPool;
    result.queryId   = queryIndex;
    result.flags     = query.query->flags();
    
    query.query->associateQuery(query.revision, result);
    m_queries.at(queryIndex) = query;
    
    m_queryRangeLength += 1;
    return result;
  }
  
  
  VkResult DxvkQueryPool::getData(
          uint32_t          queryIndex,
          uint32_t          queryCount) {
    std::array<DxvkQueryData, MaxNumQueryCountPerPool> results;
    
    // We cannot use VK_QUERY_RESULT_WAIT_BIT here since that
    // may stall the calling thread indefinitely. Instead, we
    // just assume that all queries should be available after
    // waiting for the fence that protects the command buffer.
    const VkResult status = m_vkd->vkGetQueryPoolResults(
      m_vkd->device(), m_queryPool, queryIndex, queryCount,
      sizeof(DxvkQueryData) * queryCount, results.data(),
      sizeof(DxvkQueryData), VK_QUERY_RESULT_64_BIT);
    
    if (status != VK_SUCCESS) {
      Logger::warn(str::format(
        "DxvkQueryPool: Failed to get query data for ", queryIndex,
        ":", queryCount, " with: ", status));
      
      // If retrieving query data failed, we need to fake query
      // data. In case of occlusion queries, we should return a
      // non-zero value for samples passed, so that games do not
      // accidentally omit certain geometry because of this.
      for (uint32_t i = 0; i < queryCount; i++) {
        results[i] = DxvkQueryData();
        
        if (m_queryType == VK_QUERY_TYPE_OCCLUSION)
          results[i].occlusion.samplesPassed = 1;
      }
    }
    
    // Forward query data to the query objects
    for (uint32_t i = 0; i < queryCount; i++) {
      const DxvkQueryRevision& query = m_queries.at(queryIndex + i);
      query.query->updateData(query.revision, results[i]);
    }
    
    return VK_SUCCESS;
  }
  
  
  void DxvkQueryPool::reset(const Rc<DxvkCommandList>& cmd) {
    cmd->cmdResetQueryPool(m_queryPool, 0, m_queryCount);
    
    m_queryRangeOffset = 0;
    m_queryRangeLength = 0;
  }
  
  
  DxvkQueryRange DxvkQueryPool::getActiveQueryRange() {
    DxvkQueryRange result;
    result.queryPool  = this;
    result.queryIndex = m_queryRangeOffset;
    result.queryCount = m_queryRangeLength;
    
    m_queryRangeOffset += m_queryRangeLength;
    m_queryRangeLength  = 0;
    return result;
  }
  
}