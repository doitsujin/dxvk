#include "dxvk_stats.h"

namespace dxvk {
  
  DxvkStatCounters::DxvkStatCounters() {
    this->reset();
  }
  
  
  DxvkStatCounters::~DxvkStatCounters() {
    
  }
  
  
  DxvkStatCounters DxvkStatCounters::diff(const DxvkStatCounters& other) const {
    DxvkStatCounters result;
    for (size_t i = 0; i < m_counters.size(); i++)
      result.m_counters[i] = m_counters[i] - other.m_counters[i];
    return result;
  }
  
  
  void DxvkStatCounters::merge(const DxvkStatCounters& other) {
    for (size_t i = 0; i < m_counters.size(); i++)
      m_counters[i] += other.m_counters[i];
  }
  
  
  void DxvkStatCounters::reset() {
    for (size_t i = 0; i < m_counters.size(); i++)
      m_counters[i] = 0;
  }
  
}
