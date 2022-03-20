#include "dxvk_lifetime.h"

namespace dxvk {
  
  DxvkLifetimeTracker:: DxvkLifetimeTracker() { }
  DxvkLifetimeTracker::~DxvkLifetimeTracker() { }
  
  
  void DxvkLifetimeTracker::notify() {
    for (const auto& resource : m_resources)
      resource.first->release(resource.second);

    m_notified = true;
  }


  void DxvkLifetimeTracker::reset() {
    // If this gets called without ever being submitted then
    // we should at least report the resources as unused
    if (!m_notified)
      this->notify();

    m_resources.clear();
  }
  
}