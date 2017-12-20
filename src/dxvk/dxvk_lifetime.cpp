#include "dxvk_lifetime.h"

namespace dxvk {
  
  DxvkLifetimeTracker:: DxvkLifetimeTracker() { }
  DxvkLifetimeTracker::~DxvkLifetimeTracker() { }
  
  
  void DxvkLifetimeTracker::reset() {
    for (auto i = m_resources.cbegin(); i != m_resources.cend(); i++)
      (*i)->release();
    m_resources.clear();
  }
  
}