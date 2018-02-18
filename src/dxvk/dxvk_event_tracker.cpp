#include "dxvk_event_tracker.h"

namespace dxvk {
  
  DxvkEventTracker::DxvkEventTracker() {
    
  }
  
  
  DxvkEventTracker::~DxvkEventTracker() {
    
  }
  
  
  void DxvkEventTracker::trackEvent(const DxvkEventRevision& event) {
    m_events.push_back(event);
  }
  
  
  void DxvkEventTracker::signalEvents() {
    for (const DxvkEventRevision& event : m_events)
      event.event->signal(event.revision);
  }
  
  
  void DxvkEventTracker::reset() {
    m_events.clear();
  }
  
}