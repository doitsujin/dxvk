#pragma once

#include <list>

#include "../com/com_include.h"

#include "sync_signal.h"

namespace dxvk::sync {
  
  /**
   * \brief Win32 fence
   *
   * CPU-side fence that also has the
   * ability to signal Win32 events.
   */
  class Win32Fence final : public Signal {

  public:

    Win32Fence()
    : m_value(0ull) { }

    explicit Win32Fence(uint64_t value)
    : m_value(value) { }

    /**
     * \brief Last signaled value
     * \returns Last signaled value
     */
    uint64_t value() const {
      return m_value.load(std::memory_order_acquire);  
    }

    /**
     * \brief Notifies signal
     *
     * In addition to waking up blocked threads, this
     * will also signal any queued win32 events with
     * a lower or equal value.
     * \param [in] value Value to set signal to
     */
    void signal(uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_value.store(value, std::memory_order_release);
      m_cond.notify_all();

      for (auto i = m_events.begin(); i != m_events.end(); ) {
        if (value >= i->second) {
          SetEvent(i->first);
          i = m_events.erase(i);
        } else {
          i++;
        }
      }
    }
    
    /**
     * \brief Waits for signal
     * \param [in] value The value to wait for
     */
    void wait(uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_cond.wait(lock, [this, value] {
        return value <= m_value.load(std::memory_order_acquire);
      });
    }

    /**
     * \brief Sets Win32 event on completion
     *
     * When the signal gets signaled with a value equal to or
     * greater than the given value, the event will be signaled.
     * Signals the event immediately if the last signaled value
     * is already greater than or equal to the requested value.
     * \param [in] event Win32 Event to signal
     * \param [in] value Requested signal value
     */
    void setEvent(HANDLE event, uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);

      if (value > this->value())
        m_events.push_back({ event, value });
      else
        SetEvent(event);
    }

  private:

    std::atomic<uint64_t>    m_value;
    dxvk::mutex              m_mutex;
    dxvk::condition_variable m_cond;

    std::list<std::pair<HANDLE, uint64_t>> m_events;

  };
  
}
