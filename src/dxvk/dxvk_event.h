#pragma once

#include <mutex>

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Event status
   */
  enum class DxvkEventStatus {
    Reset     = 0,
    Signaled  = 1,
  };
  
  /**
   * \brief Event
   * 
   * A CPU-side fence that will be signaled after
   * all previous Vulkan commands recorded to a
   * command buffer have finished executing.
   */
  class DxvkEvent : public RcObject {
    
  public:
    
    DxvkEvent();
    ~DxvkEvent();
    
    /**
     * \brief Resets the event
     * \returns New revision ID
     */
    uint32_t reset();
    
    /**
     * \brief Signals the event
     * \param [in] revision The revision ID
     */
    void signal(uint32_t revision);
    
    /**
     * \brief Queries event status
     * \returns Current event status
     */
    DxvkEventStatus getStatus();
    
  private:
    
    std::mutex m_mutex;
    
    DxvkEventStatus         m_status   = DxvkEventStatus::Signaled;
    uint32_t                m_revision = 0;
    
  };
  
  /**
   * \brief Event revision
   * 
   * Stores the event object and the
   * version ID for event operations.
   */
  struct DxvkEventRevision {
    Rc<DxvkEvent> event;
    uint32_t      revision;
  };
  
}