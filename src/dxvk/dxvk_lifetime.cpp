#include "dxvk_lifetime.h"

namespace dxvk {
  
  DxvkLifetimeTracker:: DxvkLifetimeTracker() { }
  DxvkLifetimeTracker::~DxvkLifetimeTracker() { }
  
  
  void DxvkLifetimeTracker::trackResource(const Rc<DxvkResource>& rc) {
    if (m_resources.insert(rc).second)
      rc->acquire();
  }
  
  
  void DxvkLifetimeTracker::reset() {
    for (auto i = m_resources.cbegin(); i != m_resources.cend(); i++)
      (*i)->release();
    m_resources.clear();
  }
  
}