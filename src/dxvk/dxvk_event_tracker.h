#pragma once

#include <vector>

#include "dxvk_event.h"

namespace dxvk {
  
  /**
   * \brief Event tracker
   */
  class DxvkEventTracker {
    
  public:
    
    DxvkEventTracker();
    ~DxvkEventTracker();
    
    /**
     * \brief Adds an event to track
     * \param [in] event The event revision
     */
    void trackEvent(const DxvkEventRevision& event);
    
    /**
     * \brief Signals tracked events
     * 
     * Retrieves query data from the query pools
     * and writes it back to the query objects.
     */
    void signalEvents();
    
    /**
     * \brief Resets event tracker
     * 
     * Releases all events from the tracker.
     * Call this after signaling the events.
     */
    void reset();
    
  private:
    
    std::vector<DxvkEventRevision> m_events;
    
  };
  
}