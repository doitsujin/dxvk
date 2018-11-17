#include "dxvk_event.h"

namespace dxvk {
  
  DxvkEvent::DxvkEvent()
  : m_packed(pack({ DxvkEventStatus::Signaled, 0u })) { }


  DxvkEvent::~DxvkEvent() {

  }
  
  
  uint32_t DxvkEvent::reset() {
    Status info;

    uint64_t packed = m_packed.load();

    do {
      info.status   = DxvkEventStatus::Reset;
      info.revision = unpack(packed).revision + 1;
    } while (!m_packed.compare_exchange_strong(packed, pack(info)));

    return info.revision;
  }
  
  
  void DxvkEvent::signal(uint32_t revision) {
    uint64_t expected = pack({ DxvkEventStatus::Reset,    revision });
    uint64_t desired  = pack({ DxvkEventStatus::Signaled, revision });
    m_packed.compare_exchange_strong(expected, desired);
  }
  
  
  DxvkEventStatus DxvkEvent::getStatus() const {
    return unpack(m_packed.load()).status;
  }


  void DxvkEvent::wait() const {
    while (this->getStatus() != DxvkEventStatus::Signaled)
      dxvk::this_thread::yield();
  }


  uint64_t DxvkEvent::pack(Status info) {
    return (uint64_t(info.revision))
         | (uint64_t(info.status) << 32);
  }


  DxvkEvent::Status DxvkEvent::unpack(uint64_t packed) {
    return { DxvkEventStatus(packed >> 32), uint32_t(packed) };
  }



  
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