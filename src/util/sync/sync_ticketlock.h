#pragma once

#include <atomic>

#include "../thread.h"

namespace dxvk::sync {
  
  /**
   * \brief Ticket spinlock
   * 
   * A fair spinlock implementation that should
   * be preferred over \ref Spinlock when one of
   * the threads accessing the lock is likely to
   * starve another.
   */
  class TicketLock {

  public:

    void lock() {
      uint32_t ticket = m_counter.fetch_add(1);

      while (m_serving.load(std::memory_order_acquire) != ticket)
        continue;
    }

    void unlock() {
      uint32_t serveNext = m_serving.load() + 1;
      m_serving.store(serveNext, std::memory_order_release);
    }

  private:

    std::atomic<uint32_t> m_counter = { 0 };
    std::atomic<uint32_t> m_serving = { 0 };

  };
  
}
