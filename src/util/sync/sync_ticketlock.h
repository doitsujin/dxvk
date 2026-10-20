#pragma once

#include <atomic>

#include "../thread.h"

#include "sync_spinlock.h"

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

      spin(200, [this, ticket] {
        return m_serving.load(std::memory_order_acquire) == ticket;
      });
    }

    void unlock() {
      uint32_t serveNext = m_serving.load() + 1;
      m_serving.store(serveNext, std::memory_order_release);
    }

    bool try_lock() {
      // Perhaps not the most useful thing ever, but if serving == counter
      // and no other thread is trying to lock, we can return immediately.
      uint32_t serving = m_serving.load(std::memory_order_relaxed);

      return m_counter.compare_exchange_strong(serving, serving + 1u,
        std::memory_order_acquire, std::memory_order_relaxed);
    }

  private:

    std::atomic<uint32_t> m_counter = { 0 };
    std::atomic<uint32_t> m_serving = { 0 };

  };
  
}
