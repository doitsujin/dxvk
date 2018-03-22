#include "dxvk_lifetime.h"

namespace dxvk {
  
  DxvkLifetimeTracker:: DxvkLifetimeTracker() { }
  DxvkLifetimeTracker::~DxvkLifetimeTracker() { }
  
  
  void DxvkLifetimeTracker::reset() {
    for (const auto& resource : m_resources)
      resource->release();
    m_resources.clear();
  }
  
}