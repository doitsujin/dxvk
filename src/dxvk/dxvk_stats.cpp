#include "dxvk_stats.h"

namespace dxvk {
  
  DxvkStatCounters:: DxvkStatCounters() { }
  DxvkStatCounters::~DxvkStatCounters() { }
  
  
  DxvkStatCounters::DxvkStatCounters(const DxvkStatCounters& other) {
    for (size_t i = 0; i < m_counters.size(); i++)
      m_counters.at(i) = other.m_counters.at(i).load();
  }
  
  
  DxvkStatCounters& DxvkStatCounters::operator = (const DxvkStatCounters& other) {
    for (size_t i = 0; i < m_counters.size(); i++)
      m_counters.at(i) = other.m_counters.at(i).load();
    return *this;
  }
  
  
  DxvkStatCounters DxvkStatCounters::delta(const DxvkStatCounters& oldState) const {
    DxvkStatCounters result;
    for (size_t i = 0; i < m_counters.size(); i++)
      result.m_counters.at(i) = m_counters.at(i) - oldState.m_counters.at(i);;
    return result;
  }
  
  
  void DxvkStatCounters::addCounters(const DxvkStatCounters& counters) {
    for (size_t i = 0; i < m_counters.size(); i++)
      m_counters.at(i) += counters.m_counters.at(i);
  }
  
  
  void DxvkStatCounters::clear() {
    for (size_t i = 0; i < m_counters.size(); i++)
      m_counters.at(i) = 0;
  }
  
}
