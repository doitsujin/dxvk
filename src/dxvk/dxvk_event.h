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
    DxvkEventStatus getStatus() const;
    
    /**
     * \brief Waits for event to get signaled
     * 
     * Blocks the calling thread until another
     * thread calls \ref signal for the current
     * revision of the event.
     */
    void wait() const;

  private:
    
    struct Status {
      DxvkEventStatus status   = DxvkEventStatus::Signaled;
      uint32_t        revision = 0;
    };

    // Packed status and revision
    std::atomic<uint64_t> m_packed;

    static uint64_t pack(Status info);
    static Status unpack(uint64_t packed);
    
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