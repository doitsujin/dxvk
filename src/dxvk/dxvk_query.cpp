#include "dxvk_query.h"

namespace dxvk {
  
  DxvkQuery::DxvkQuery(
    VkQueryType         type,
    VkQueryControlFlags flags)
  : m_type(type), m_flags(flags) {
    
  }
  
  
  DxvkQuery::~DxvkQuery() {
    
  }
  
  
  uint32_t DxvkQuery::reset() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    m_status = DxvkQueryStatus::Reset;
    m_data = DxvkQueryData { };
    
    m_queryIndex = 0;
    m_queryCount = 0;
    
    return ++m_revision;
  }
  
  
  DxvkQueryStatus DxvkQuery::getData(DxvkQueryData& data) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (m_status == DxvkQueryStatus::Available)
      data = m_data;
    
    return m_status;
  }
  
  
  DxvkQueryHandle DxvkQuery::getHandle() {
    return m_handle;
  }
  
  
  void DxvkQuery::beginRecording(uint32_t revision) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (m_revision == revision)
      m_status = DxvkQueryStatus::Active;
  }
  
  
  void DxvkQuery::endRecording(uint32_t revision) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (m_revision == revision) {
      if (m_queryIndex < m_queryCount) {
        m_status = DxvkQueryStatus::Pending;
      } else {
        m_status = DxvkQueryStatus::Available;
      }
      
      m_handle = DxvkQueryHandle();
    }
  }
  
  
  void DxvkQuery::associateQuery(uint32_t revision, DxvkQueryHandle handle) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (m_revision == revision)
      m_queryCount += 1;
    
    // Assign the handle either way as this
    // will be used by the DXVK context.
    m_handle = handle;
  }
  
  
  void DxvkQuery::updateData(
          uint32_t       revision,
    const DxvkQueryData& data) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (m_revision == revision) {
      switch (m_type) {
        case VK_QUERY_TYPE_OCCLUSION:
          m_data.occlusion.samplesPassed += data.occlusion.samplesPassed;
          break;
        
        case VK_QUERY_TYPE_TIMESTAMP:
          m_data.timestamp.time = data.timestamp.time;
          break;
        
        case VK_QUERY_TYPE_PIPELINE_STATISTICS:
          m_data.statistic.iaVertices       += data.statistic.iaVertices;
          m_data.statistic.iaPrimitives     += data.statistic.iaPrimitives;
          m_data.statistic.vsInvocations    += data.statistic.vsInvocations;
          m_data.statistic.gsInvocations    += data.statistic.gsInvocations;
          m_data.statistic.gsPrimitives     += data.statistic.gsPrimitives;
          m_data.statistic.clipInvocations  += data.statistic.clipInvocations;
          m_data.statistic.clipPrimitives   += data.statistic.clipPrimitives;
          m_data.statistic.fsInvocations    += data.statistic.fsInvocations;
          m_data.statistic.tcsPatches       += data.statistic.tcsPatches;
          m_data.statistic.tesInvocations   += data.statistic.tesInvocations;
          m_data.statistic.csInvocations    += data.statistic.csInvocations;
          break;
        
        default:
          Logger::err(str::format("DxvkQuery: Unhandled query type: ", m_type));
      }
      
      if (++m_queryIndex == m_queryCount && m_status == DxvkQueryStatus::Pending)
        m_status = DxvkQueryStatus::Available;
    }
  }
  
}