#pragma once

#include <condition_variable>
#include <mutex>

#include "dxvk_include.h"

namespace dxvk {
  
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
    void signalEvent(uint32_t revision);
    
    /**
     * \brief Queries event status
     * \returns Current event status
     */
    DxvkEventStatus getStatus();
    
  private:
    
    std::mutex              m_mutex;
    std::condition_variable m_signal;
    
    DxvkEventStatus         m_status   = DxvkEventStatus::Reset;
    uint32_t                m_revision = 0;
    
  };
  
}