#include "dxvk_lifetime.h"

namespace dxvk {
  
  DxvkLifetimeTracker:: DxvkLifetimeTracker() { }
  DxvkLifetimeTracker::~DxvkLifetimeTracker() { }
  
  
  void DxvkLifetimeTracker::notify() {
    m_resources.clear();
  }


  void DxvkLifetimeTracker::reset() {
    m_resources.clear();
  }
  
}