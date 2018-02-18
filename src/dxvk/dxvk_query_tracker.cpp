#include "dxvk_query_tracker.h"

namespace dxvk {
  
  DxvkQueryTracker:: DxvkQueryTracker() { }
  DxvkQueryTracker::~DxvkQueryTracker() { }
  
  
  void DxvkQueryTracker::trackQueryRange(DxvkQueryRange&& queryRange) {
    m_queries.push_back(std::move(queryRange));
  }
  
  
  void DxvkQueryTracker::writeQueryData() {
    for (const DxvkQueryRange& curr : m_queries)
      curr.queryPool->getData(curr.queryIndex, curr.queryCount);
  }
  
  
  void DxvkQueryTracker::reset() {
    m_queries.clear();
  }
  
}